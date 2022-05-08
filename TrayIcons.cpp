#include "TrayIcons.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

#include <SingleApplication>

#include "ClockifyManager.h"
#include "JsonHelper.h"
#include "Project.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "TimeCampManager.h"
#include "User.h"
#include "version.h"

TrayIcons::TrayIcons(QObject *parent) : QObject{parent}, m_timerRunning{new QSystemTrayIcon}
{
    while (Settings::instance()->apiKey().isEmpty())
    {
        bool ok{false};
        QString newKey = QInputDialog::getText(
            nullptr, tr("API key"), tr("Enter your Clockify API key:"), QLineEdit::Normal, QString{}, &ok);
        if (!ok)
        {
            m_valid = false;
            return;
        }
        else
            Settings::instance()->setApiKey(newKey);
    }

    if (Settings::instance()->timeService() == QStringLiteral("com.clockify"))
        initializeManager<ClockifyManager>();
    else if (Settings::instance()->timeService() == QStringLiteral("com.timecamp"))
        initializeManager<TimeCampManager>();
    else
    {
        // since the service is invalid, we'll just select Clockify...
        Settings::instance()->setTimeService("com.clockify");
        initializeManager<ClockifyManager>();
    }

    auto fixApiKey = [&] {
        while (!m_manager->isValid())
        {
            bool ok{false};
            QString newKey = QInputDialog::getText(nullptr,
                                                   tr("API key"),
                                                   tr("The API key is incorrect or invalid. Please enter a valid API key:"),
                                                   QLineEdit::Normal,
                                                   QString{},
                                                   &ok);
            if (!ok)
                return false;
            Settings::instance()->setApiKey(newKey);
            m_manager->setApiKey(newKey);
        }

        return true;
    };
    if (!fixApiKey())
    {
        m_valid = false;
        return;
    }

    connect(m_manager, &ClockifyManager::invalidated, this, [this, fixApiKey]() {
        if (!fixApiKey())
        {
            m_valid = false;
            return;
        }
    });

    m_user = m_manager->getApiKeyOwner();
    if (Settings::instance()->workspaceId().isEmpty())
        Settings::instance()->setWorkspaceId(m_user.workspaceId());
    m_manager->setWorkspaceId(Settings::instance()->workspaceId());
    if (!m_user.isValid()) [[unlikely]]
    {
        QMessageBox::warning(nullptr, tr("Fatal error"), tr("Could not load user!"));
        m_valid = false;
        return;
    }

    while ((Settings::instance()->projectId().isEmpty() && !Settings::instance()->useLastProject()) ||
           Settings::instance()->breakTimeId().isEmpty())
    {
        QMessageBox::information(nullptr,
                                 tr("Select a project"),
                                 tr("Please select a default project and break project in the following dialog."));
        getNewProjectId();
    }

    if (Settings::instance()->useSeparateBreakTime())
        m_runningJob = new QSystemTrayIcon;

    setUpTrayIcons();

    m_eventLoop.setInterval(Settings::instance()->eventLoopInterval());
    m_eventLoop.setSingleShot(false);
    m_eventLoop.callOnTimeout(this, &TrayIcons::updateTrayIcons);
    m_eventLoop.start();

    connect(Settings::instance(), &Settings::eventLoopIntervalChanged, this, [this] {
        m_eventLoop.setInterval(Settings::instance()->eventLoopInterval());
    });
}

TrayIcons::~TrayIcons()
{
    delete m_timerRunning;
    if (m_runningJob)
        delete m_runningJob;
}

void TrayIcons::show()
{
    m_timerRunning->show();
    if (m_runningJob)
        m_runningJob->show();
}

Project TrayIcons::defaultProject()
{
    QString projectId;
    QString description;
    int pageNum{m_manager->paginationStartsAt()};
    bool itemsLeftToLoad{true};
    auto entries = m_user.getTimeEntries(pageNum);

    if (!Settings::instance()->useLastProject())
        projectId = Settings::instance()->projectId();
    else
    {
        bool projectIdLoaded{false};

        do
        {
            if (m_manager->timeEntriesSortOrder() == Qt::AscendingOrder)
                std::reverse(entries.begin(), entries.end());

            for (const auto &entry : entries) // (m_manager->timeEntriesSortOrder() == Qt::DescendingOrder ? entries :
                                              // std::ranges::reverse(entries.begin(), entries.end())))
            {
                if (entry.project().id().isEmpty()) [[unlikely]]
                {
                    std::cerr << "Error: getting project id failed\n";
                    continue; // no project id to see here, move along
                }
                else if (entry.project().id() != Settings::instance()->breakTimeId())
                {
                    projectId = entry.project().id();
                    projectIdLoaded = true;
                    break;
                }
            }
            if (!m_manager->supportedPagination().testFlag(AbstractTimeServiceManager::Pagination::TimeEntries))
                break;

            auto newEntries = m_user.getTimeEntries(++pageNum);
            if (newEntries.empty())
                itemsLeftToLoad = false;
            else
            {
                if (m_manager->timeEntriesSortOrder() == Qt::AscendingOrder)
                    std::reverse(newEntries.begin(), newEntries.end());
                entries.append(newEntries);
            }
        } while (itemsLeftToLoad || !projectIdLoaded);

        // when all else fails, use the first extant project
        if (!projectIdLoaded) [[unlikely]]
            projectId = m_manager->projects().first().id();
    }

    if (Settings::instance()->disableDescription())
        ; // description is already empty, so we will fall through
    else if (!Settings::instance()->useLastDescription())
        description = Settings::instance()->description();
    else
    {
        bool descriptionLoaded{false};

        do
        {
            for (const auto &entry : entries)
            {
                if (entry.project().id() != Settings::instance()->breakTimeId())
                {
                    description = entry.project().description();
                    descriptionLoaded = true;
                    break;
                }
            }

            auto newEntries = m_user.getTimeEntries(++pageNum);
            if (newEntries.empty())
                itemsLeftToLoad = false;
            else
            {
                if (m_manager->timeEntriesSortOrder() == Qt::AscendingOrder)
                    std::reverse(newEntries.begin(), newEntries.end());
                entries.append(newEntries);
            }
        } while (itemsLeftToLoad || !descriptionLoaded);

        if (!descriptionLoaded) [[unlikely]]
            description = m_manager->projects().first().description();
    }

    return Project{projectId, m_manager->projectName(projectId), description};
}

void TrayIcons::setUser(const User &user)
{
    m_user = user;
}

void TrayIcons::updateTrayIcons()
{
    if (!m_manager->isConnectedToInternet()) [[unlikely]]
        setTimerState(TimerState::Offline);
    else if (m_user.hasRunningTimeEntry())
    {
        try
        {
            if (auto runningEntry = m_user.getRunningTimeEntry();
                runningEntry.project().id() == Settings::instance()->breakTimeId())
                setTimerState(TimerState::OnBreak);
            else
            {
                setTimerState(TimerState::Running);
                m_currentRunningJobId = runningEntry.id();
            }
        }
        catch (const std::exception &ex)
        {
            std::cerr << "Could not load running time entry: " << ex.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "Could not load running time entry" << std::endl;
        }
    }
    else
        setTimerState(TimerState::NotRunning);
}

void TrayIcons::getNewProjectId()
{
    SettingsDialog d{m_manager,
                     {{QStringLiteral("Clockify"), QStringLiteral("com.clockify")},
                      {QStringLiteral("TimeCamp"), QStringLiteral("com.timecamp")}}};
    d.switchToPage(SettingsDialog::Pages::ProjectPage);
    d.exec();
}

void TrayIcons::showAboutDialog()
{
    // put this into a variable to handle this gonzo string more nicely
    auto licenseInfo = tr("ClockifyTrayIcons %1 copyright © 2022. Licensed "
                          "under the MIT license.\n\nIcons from "
                          "[1RadicalOne](https://commons.wikimedia.org/wiki/User:1RadicalOne) "
                          "(light icons, licensed [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/))"
                          " and [Microsoft](https://github.com/microsoft/fluentui-system-icons) "
                          "(power icon, licensed [MIT](https://mit-license.org)).")
                           .arg(VERSION_STR);

    QDialog dialog;

    auto layout = new QVBoxLayout{&dialog};

    auto infoLabel = new QLabel{licenseInfo, &dialog};
    infoLabel->setTextFormat(Qt::MarkdownText);
    infoLabel->setWordWrap(true);
    infoLabel->setOpenExternalLinks(true);
    layout->addWidget(infoLabel);

    auto bb = new QDialogButtonBox{QDialogButtonBox::Ok, &dialog};
    connect(bb->addButton(tr("Show license"), QDialogButtonBox::ActionRole), &QPushButton::clicked, this, [this, &dialog] {
        showLicenseDialog(&dialog);
    });
    connect(bb, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    layout->addWidget(bb);

    dialog.setLayout(layout);
    dialog.setWindowTitle(tr("About ") + qApp->applicationName());
    dialog.resize(500, dialog.heightForWidth(500));
    dialog.move(dialog.screen()->geometry().width() / 2 - dialog.width() / 2,
                dialog.screen()->geometry().height() / 2 - dialog.height() / 2);
    dialog.exec();
}

void TrayIcons::showLicenseDialog(QWidget *parent)
{
    QDialog dialog{parent};
    auto layout = new QVBoxLayout{&dialog};
    auto licenseView = new QTextBrowser{&dialog};

    QFile licenseFile{QStringLiteral(":/LICENSE")};
    if (licenseFile.open(QIODevice::ReadOnly)) [[likely]]
        licenseView->setText(licenseFile.readAll());
    else [[unlikely]] // this really should never happen, but just in case...
        licenseView->setMarkdown(tr("Error: could not load the license. Please read the license on "
                                    "[GitHub](https://github.com/ChristianLightServices/ClockifyTrayIcons/blob/master/"
                                    "LICENSE)."));

    licenseView->setOpenExternalLinks(true);
    licenseView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(licenseView);

    auto bb = new QDialogButtonBox{QDialogButtonBox::Ok, &dialog};
    connect(bb, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(bb);

    dialog.setWindowTitle(tr("MIT license"));
    dialog.setModal(true);
    dialog.resize(600, 600);
    dialog.move(dialog.screen()->geometry().width() / 2 - dialog.width() / 2,
                dialog.screen()->geometry().height() / 2 - dialog.height() / 2);
    dialog.exec();
}

void TrayIcons::setEventLoopInterval(int interval)
{
    Settings::instance()->setEventLoopInterval(interval);
    m_eventLoop.setInterval(Settings::instance()->eventLoopInterval());
}

void TrayIcons::setUpTrayIcons()
{
    auto m_timerRunningMenu = new QMenu;
    connect(m_timerRunningMenu->addAction(tr("Start")), &QAction::triggered, this, [this]() {
        if (m_manager->isConnectedToInternet() == false) [[unlikely]]
            m_timerRunning->showMessage(tr("Internet connection lost"),
                                        tr("The request could not be completed because the internet connection is down."));

        if (!m_user.hasRunningTimeEntry()) [[likely]]
        {
            auto project = defaultProject();
            m_user.startTimeEntry(project.id(), project.description());
            updateTrayIcons();
        }
    });
    connect(m_timerRunningMenu->addAction(tr("Stop")), &QAction::triggered, this, [this]() {
        if (m_manager->isConnectedToInternet() == false) [[unlikely]]
            m_timerRunning->showMessage(tr("Internet connection lost"),
                                        tr("The request could not be completed because the internet connection is down."));

        if (m_user.hasRunningTimeEntry()) [[likely]]
        {
            m_user.stopCurrentTimeEntry();
            updateTrayIcons();
        }
    });
    connect(m_timerRunningMenu->addAction(tr("Settings")), &QAction::triggered, this, [this] {
        SettingsDialog d{m_manager,
                         {{QStringLiteral("Clockify"), QStringLiteral("com.clockify")},
                          {QStringLiteral("TimeCamp"), QStringLiteral("com.timecamp")}}};
        d.exec();
    });
    connect(m_timerRunningMenu->addAction(tr("Go to the %1 website").arg(m_manager->serviceName())),
            &QAction::triggered,
            this,
            [this]() { QDesktopServices::openUrl(m_manager->timeTrackerWebpageUrl()); });
    connect(
        m_timerRunningMenu->addAction(tr("About Qt")), &QAction::triggered, this, []() { QMessageBox::aboutQt(nullptr); });
    connect(m_timerRunningMenu->addAction(tr("About")), &QAction::triggered, this, &TrayIcons::showAboutDialog);
    connect(m_timerRunningMenu->addAction(tr("Quit")), &QAction::triggered, qApp, &QApplication::quit);

    m_timerRunning->setContextMenu(m_timerRunningMenu);
    connect(m_timerRunning, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        m_eventLoop.stop();

        if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick) [[unlikely]]
        {
            m_eventLoop.start();
            return;
        }

        if (m_manager->isConnectedToInternet() == false) [[unlikely]]
        {
            m_timerRunning->showMessage(tr("Internet connection lost"),
                                        tr("The request could not be completed because the internet connection is down."));
            m_eventLoop.start();
            return;
        }

        if (m_user.hasRunningTimeEntry())
            m_user.stopCurrentTimeEntry();
        else
        {
            auto project = defaultProject();
            m_user.startTimeEntry(project.id(), project.description());
        }

        m_eventLoop.start();
    });

    if (Settings::instance()->useSeparateBreakTime())
    {
        auto m_runningJobMenu = new QMenu;
        connect(m_runningJobMenu->addAction(tr("Break")), &QAction::triggered, this, [this]() {
            if (m_manager->isConnectedToInternet() == false) [[unlikely]]
            {
                m_timerRunning->showMessage(tr("Internet connection lost"),
                                            tr("The request could not be completed because the internet connection is "
                                               "down."));
                return;
            }

            QDateTime start = QDateTime::currentDateTimeUtc();

            if (m_user.hasRunningTimeEntry())
            {
                try
                {
                    if (m_user.getRunningTimeEntry().project().id() == Settings::instance()->breakTimeId())
                        return;
                }
                catch (const std::exception &)
                {}

                start = m_user.stopCurrentTimeEntry();
            }
            m_user.startTimeEntry(Settings::instance()->breakTimeId(), start);
            updateTrayIcons();
        });
        connect(m_runningJobMenu->addAction(tr("Resume work")), &QAction::triggered, this, [this]() {
            if (m_manager->isConnectedToInternet() == false) [[unlikely]]
            {
                m_timerRunning->showMessage(tr("Internet connection lost"),
                                            tr("The request could not be completed because the internet connection is "
                                               "down."));
                return;
            }

            QDateTime start = QDateTime::currentDateTimeUtc();

            if (m_user.hasRunningTimeEntry())
            {
                try
                {
                    if (m_user.getRunningTimeEntry().project().id() != Settings::instance()->breakTimeId())
                        return;
                }
                catch (const std::exception &)
                {}

                start = m_user.stopCurrentTimeEntry();
            }
            auto project = defaultProject();
            m_user.startTimeEntry(project.id(), project.description(), start);
            updateTrayIcons();
        });
        connect(m_runningJobMenu->addAction(tr("Settings")), &QAction::triggered, this, [this] {
            SettingsDialog d{m_manager,
                             {{QStringLiteral("Clockify"), QStringLiteral("com.clockify")},
                              {QStringLiteral("TimeCamp"), QStringLiteral("com.timecamp")}}};
            d.exec();
        });
        connect(m_runningJobMenu->addAction(tr("Go to the %1 website").arg(m_manager->serviceName())),
                &QAction::triggered,
                this,
                [this]() { QDesktopServices::openUrl(m_manager->timeTrackerWebpageUrl()); });
        connect(
            m_runningJobMenu->addAction(tr("About Qt")), &QAction::triggered, this, []() { QMessageBox::aboutQt(nullptr); });
        connect(m_runningJobMenu->addAction(tr("About")), &QAction::triggered, this, &TrayIcons::showAboutDialog);
        connect(m_runningJobMenu->addAction(tr("Quit")), &QAction::triggered, qApp, &QApplication::quit);

        m_runningJob->setContextMenu(m_runningJobMenu);
        connect(m_runningJob, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            m_eventLoop.stop();

            if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick) [[unlikely]]
            {
                m_eventLoop.start();
                return;
            }

            if (m_manager->isConnectedToInternet() == false) [[unlikely]]
            {
                m_timerRunning->showMessage(tr("Internet connection lost"),
                                            tr("The request could not be completed because the internet connection is "
                                               "down."));
                m_eventLoop.start();
                return;
            }

            if (m_user.hasRunningTimeEntry())
            {
                if (m_user.getRunningTimeEntry().project().id() == Settings::instance()->breakTimeId())
                {
                    auto time = m_user.stopCurrentTimeEntry();
                    auto project = defaultProject();
                    m_user.startTimeEntry(project.id(), project.description(), time);
                }
                else
                {
                    auto time = m_user.stopCurrentTimeEntry();
                    m_user.startTimeEntry(Settings::instance()->breakTimeId(), time);
                }
            }
            else
            {
                auto project = defaultProject();
                m_user.startTimeEntry(project.id(), project.description());
            }

            m_eventLoop.start();
        });
    }

    connect(m_manager, &ClockifyManager::internetConnectionChanged, this, [this](bool) { updateTrayIcons(); });

    connect(this, &TrayIcons::jobEnded, this, [this] {
        if (!Settings::instance()->showDurationNotifications())
            return;

        auto jobs = m_user.getTimeEntries(1, 1);
        if (jobs.isEmpty())
            return;
        auto job{m_manager->timeEntriesSortOrder() == Qt::DescendingOrder ? jobs.first() : jobs.last()};
        if (!m_currentRunningJobId.isEmpty() && job.id() != m_currentRunningJobId)
            return;
        m_currentRunningJobId.clear();

        QTime duration{QTime::fromMSecsSinceStartOfDay(static_cast<int>(job.start().msecsTo(job.end())))};
        QString timeString{tr("%n minute(s)", nullptr, duration.minute())};
        if (duration.hour() > 0)
            timeString.prepend(tr("%n hour(s) and ", nullptr, duration.hour()));
        else
            timeString.append(tr(" and %n second(s)", nullptr, duration.second()));
        m_timerRunning->showMessage(tr("Job ended"),
                                    tr("You worked %1 on %2").arg(timeString, job.project().name()),
                                    QSystemTrayIcon::Information,
                                    5000);
    });

    updateTrayIcons();
}

void TrayIcons::setTimerState(const TimerState state)
{
    if (state == m_timerState || state == TimerState::StateUnset)
        return;

    switch (state)
    {
    case TimerState::Running:
        m_timerRunning->setToolTip(tr("%1 is running").arg(m_manager->serviceName()));
        m_timerRunning->setIcon(
            QIcon{m_runningJob ? QStringLiteral(":/icons/greenpower.png") : QStringLiteral(":/icons/greenlight.png")});
        if (m_runningJob)
        {
            m_runningJob->setToolTip(tr("You are working"));
            m_runningJob->setIcon(QIcon{":/icons/greenlight.png"});
        }
        break;
    case TimerState::OnBreak:
        m_timerRunning->setToolTip(tr("%1 is running").arg(m_manager->serviceName()));
        m_timerRunning->setIcon(
            QIcon{m_runningJob ? QStringLiteral(":/icons/greenpower.png") : QStringLiteral(":/icons/greenlight.png")});
        if (m_runningJob) // this *should* always be true, but just in case...
        {
            m_runningJob->setToolTip(tr("You are on break"));
            m_runningJob->setIcon(QIcon{":/icons/yellowlight.png"});
        }

        if (m_timerState == TimerState::Running)
            emit jobEnded();
        break;
    case TimerState::NotRunning:
        m_timerRunning->setToolTip(tr("%1 is not running").arg(m_manager->serviceName()));
        m_timerRunning->setIcon(
            QIcon{m_runningJob ? QStringLiteral(":/icons/redpower.png") : QStringLiteral(":/icons/redlight.png")});
        if (m_runningJob)
        {
            m_runningJob->setToolTip(tr("You are not working"));
            m_runningJob->setIcon(QIcon{":/icons/redlight.png"});
        }

        if (m_timerState == TimerState::Running)
            emit jobEnded();
        break;
    case TimerState::Offline:
        m_timerRunning->setToolTip(tr("You are offline"));
        m_timerRunning->setIcon(
            QIcon{m_runningJob ? QStringLiteral(":/icons/graypower.png") : QStringLiteral(":/icons/graylight.png")});
        if (m_runningJob)
        {
            m_runningJob->setToolTip(tr("You are offline"));
            m_runningJob->setIcon(QIcon{":/icons/graylight.png"});
        }
        break;
    default:
        break;
    }

    m_timerState = state;
    emit timerStateChanged();
}

template<TimeManager Manager> void TrayIcons::initializeManager()
{
    m_manager = new Manager{Settings::instance()->apiKey().toUtf8()};
}
