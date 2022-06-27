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
#include "DeveloperTools.h"
#include "JsonHelper.h"
#include "ModifyJobDialog.h"
#include "Project.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "TimeCampManager.h"
#include "User.h"
#include "version.h"

TrayIcons::TrayIcons(QObject *parent)
    : QObject{parent},
      m_timerRunning{new QSystemTrayIcon}
{
    while (Settings::instance()->timeService().isEmpty())
    {
        bool ok{false};
        QString service = QInputDialog::getItem(nullptr,
                                                tr("Time service"),
                                                tr("Choose which time service to use:"),
                                                QStringList{} << QStringLiteral("Clockify") << QStringList("TimeCamp"),
                                                0,
                                                false,
                                                &ok);
        if (!ok)
        {
            m_valid = false;
            return;
        }
        else if (service == QStringLiteral("Clockify"))
            Settings::instance()->setTimeService(QStringLiteral("com.clockify"));
        else if (service == QStringLiteral("TimeCamp"))
        {
            Settings::instance()->setTimeService(QStringLiteral("com.timecamp"));
            Settings::instance()->setEventLoopInterval(15000);
        }
    }

    while (Settings::instance()->apiKey().isEmpty())
    {
        bool ok{false};
        QString newKey =
            QInputDialog::getText(nullptr, tr("API key"), tr("Enter your API key:"), QLineEdit::Normal, QString{}, &ok);
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
                                                   tr("The API key seems to be incorrect or invalid. Please enter a valid "
                                                      "API key:"),
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
    {
        auto workspaces = m_manager->workspaces();
        QStringList names;
        for (const auto &w : workspaces)
            names << w.name();
        while (Settings::instance()->workspaceId().isEmpty())
        {
            bool ok{false};
            QString workspace = QInputDialog::getItem(
                nullptr, tr("Workspace"), tr("Choose which workspace to track time on:"), names, 0, false, &ok);
            if (!ok)
            {
                m_valid = false;
                return;
            }

            for (const auto &w : workspaces)
                if (w.name() == workspace)
                {
                    Settings::instance()->setWorkspaceId(w.id());
                    break;
                }
        }
    }
    m_manager->setWorkspaceId(Settings::instance()->workspaceId());

    if (!m_user.isValid()) [[unlikely]]
    {
        QMessageBox::warning(nullptr, tr("Fatal error"), tr("Could not load user!"));
        m_valid = false;
        return;
    }

    while ((!Settings::instance()->useLastProject() && Settings::instance()->projectId().isEmpty()) ||
           (Settings::instance()->useSeparateBreakTime() && Settings::instance()->breakTimeId().isEmpty()))
    {
        QMessageBox::information(
            nullptr, tr("Select a project"), tr("Please select a default project in the following dialog."));
        getNewProjectId();
    }

    if (Settings::instance()->useSeparateBreakTime())
        m_runningJob = new QSystemTrayIcon;

    setUpTrayIcons();

    m_eventLoop.setInterval(Settings::instance()->eventLoopInterval());
    m_eventLoop.setSingleShot(false);
    m_eventLoop.callOnTimeout(this, &TrayIcons::updateTrayIcons);
    m_eventLoop.start();

    // check for completed work time every hour
    m_alertOnTimeUpTimer.setInterval(1000 * 60 * 60);
    m_alertOnTimeUpTimer.setSingleShot(false);
    m_alertOnTimeUpTimer.callOnTimeout(this, &TrayIcons::checkForFinishedWeek);
    m_alertOnTimeUpTimer.start();

    connect(Settings::instance(), &Settings::eventLoopIntervalChanged, this, [this] {
        m_eventLoop.setInterval(Settings::instance()->eventLoopInterval());
    });
    connect(Settings::instance(), &Settings::quickStartProjectsLoadingChanged, this, [this] { updateQuickStartList(); });

    connect(m_manager, &AbstractTimeServiceManager::ratelimited, this, [this](bool ratelimited) {
        m_ratelimited = ratelimited;
        if (m_ratelimited)
        {
            // go into "sleep mode" to try to snap out of ratelimiting
            m_eventLoop.setInterval(30000);
            setTimerState(TimerState::Ratelimited);
        }
        else
            m_eventLoop.setInterval(Settings::instance()->eventLoopInterval());
    });

    connect(this, &TrayIcons::jobStarted, this, [this] {
        m_timerState = TimerState::StateUnset;
        updateTrayIcons();
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
    checkForFinishedWeek();
}

Project TrayIcons::defaultProject()
{
    QString projectId;
    QString description;
    int pageNum{m_manager->paginationStartsAt()};
    bool itemsLeftToLoad{true};
    QVector<TimeEntry> entries;

    if (!Settings::instance()->useLastProject())
        projectId = Settings::instance()->projectId();
    else
    {
        bool projectIdLoaded{false};
        entries = m_user.getTimeEntries(pageNum);

        do
        {
            for (const auto &entry : entries)
            {
                if (entry.project().id().isEmpty()) [[unlikely]]
                {
                    std::cerr << "Error: getting project id failed\n";
                    continue; // no project id to see here, move along
                }
                else if (!Settings::instance()->useSeparateBreakTime() ||
                         entry.project().id() != Settings::instance()->breakTimeId())
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
                entries.append(newEntries);
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
                if (!Settings::instance()->useSeparateBreakTime() ||
                    entry.project().id() != Settings::instance()->breakTimeId())
                {
                    description = entry.project().description();
                    descriptionLoaded = true;
                    break;
                }
            }
            if (!m_manager->supportedPagination().testFlag(AbstractTimeServiceManager::Pagination::TimeEntries))
                break;

            auto newEntries = m_user.getTimeEntries(++pageNum);
            if (newEntries.empty())
                itemsLeftToLoad = false;
            else
                entries.append(newEntries);
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
    else if (auto runningEntry = m_user.getRunningTimeEntry(); runningEntry)
    {
        try
        {
            m_currentRunningJob = *runningEntry;
            if (Settings::instance()->useSeparateBreakTime() &&
                runningEntry->project().id() == Settings::instance()->breakTimeId())
                setTimerState(TimerState::OnBreak);
            else
                setTimerState(TimerState::Running);
            // At this point, the previous job will have been notified for, so it's safe to overwrite it
            m_jobToBeNotified = *runningEntry;
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
    auto aboutText = tr("TimeLight %1 copyright © 2022. Licensed "
                        "under the MIT license.\n\nIcons from "
                        "[1RadicalOne](https://commons.wikimedia.org/wiki/User:1RadicalOne) "
                        "(light icons, licensed [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/))"
                        " and [Microsoft](https://github.com/microsoft/fluentui-system-icons) "
                        "(power icon, licensed [MIT](https://mit-license.org)).")
                         .arg(VERSION_STR);

    QDialog dialog;

    auto layout = new QVBoxLayout{&dialog};

    auto infoLabel = new QLabel{aboutText, &dialog};
    infoLabel->setTextFormat(Qt::MarkdownText);
    infoLabel->setWordWrap(true);
    infoLabel->setOpenExternalLinks(true);
    layout->addWidget(infoLabel);

    auto bb = new QDialogButtonBox{QDialogButtonBox::Ok, &dialog};
    connect(bb->addButton(tr("Show license"), QDialogButtonBox::ActionRole), &QPushButton::clicked, this, [this, &dialog] {
        showLicenseDialog(&dialog);
    });
    connect(bb->addButton(tr("Show source code"), QDialogButtonBox::ActionRole), &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl{QStringLiteral("https://github.com/ChristianLightServices/TimeLight")});
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
                                    "[GitHub](https://github.com/ChristianLightServices/TimeLight/blob/master/"
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
    updateQuickStartList();

    auto addMenuActions = [this](QMenu *menu) {
        menu->addMenu(m_quickStartMenu);
        auto modifyJob = menu->addAction(tr("Modify current job"));
        connect(modifyJob, &QAction::triggered, this, [this] {
            if (auto e = m_user.getRunningTimeEntry(); e)
            {
                auto dialog = new ModifyJobDialog{m_manager, *e};
                dialog->show();
                connect(dialog, &QDialog::finished, this, [this, dialog](int result) {
                    if (result == QDialog::Accepted)
                        m_user.modifyTimeEntry(std::move(dialog->entry()), true);
                    dialog->deleteLater();
                });
            }
        });
        auto cancel = menu->addAction(tr("Cancel current job"));
        connect(cancel, &QAction::triggered, this, [this] {
            if (auto e = m_user.getRunningTimeEntry(); e)
            {
                if (QMessageBox::question(nullptr,
                                          tr("Cancel job"),
                                          tr("Are you sure you want to cancel the current job?")) == QMessageBox::Yes)
                {
                    m_user.deleteTimeEntry(*e, true);
                    updateTrayIcons();
                }
            }
        });
        modifyJob->setDisabled(true);
        cancel->setDisabled(true);
        connect(this, &TrayIcons::jobEnded, this, [modifyJob, cancel] {
            modifyJob->setDisabled(true);
            cancel->setDisabled(true);
        });
        connect(this, &TrayIcons::jobStarted, this, [modifyJob, cancel] {
            modifyJob->setDisabled(false);
            cancel->setDisabled(false);
        });

        connect(menu->addAction(tr("Settings")), &QAction::triggered, this, [this] {
            SettingsDialog d{m_manager,
                             {{QStringLiteral("Clockify"), QStringLiteral("com.clockify")},
                              {QStringLiteral("TimeCamp"), QStringLiteral("com.timecamp")}}};
            d.exec();
        });

        auto devTools = menu->addAction(tr("Developer tools"));
        devTools->setVisible(Settings::instance()->developerMode());
        connect(devTools, &QAction::triggered, this, [this] {
            DeveloperTools d{m_manager};
            d.exec();
        });
        connect(Settings::instance(), &Settings::developerModeChanged, devTools, [devTools] {
            devTools->setVisible(Settings::instance()->developerMode());
        });

        connect(menu->addAction(tr("Go to the %1 website").arg(m_manager->serviceName())),
                &QAction::triggered,
                this,
                [this]() { QDesktopServices::openUrl(m_manager->timeTrackerWebpageUrl()); });
        connect(menu->addAction(tr("About Qt")), &QAction::triggered, this, []() { QMessageBox::aboutQt(nullptr); });
        connect(menu->addAction(tr("About")), &QAction::triggered, this, &TrayIcons::showAboutDialog);
        connect(menu->addAction(tr("Quit")), &QAction::triggered, qApp, &QApplication::quit);
    };

    auto timerRunningMenu = new QMenu;
    connect(timerRunningMenu->addAction(tr("Start")), &QAction::triggered, this, [this]() {
        if (!m_manager->isConnectedToInternet()) [[unlikely]]
            showOfflineNotification();

        if (!m_user.getRunningTimeEntry()) [[likely]]
        {
            auto project = defaultProject();
            m_user.startTimeEntry(project.id(), project.description());
            updateTrayIcons();
        }
    });
    connect(timerRunningMenu->addAction(tr("Stop")), &QAction::triggered, this, [this]() {
        if (!m_manager->isConnectedToInternet()) [[unlikely]]
            showOfflineNotification();

        if (m_user.getRunningTimeEntry()) [[likely]]
        {
            m_user.stopCurrentTimeEntry();
            updateTrayIcons();
        }
    });
    addMenuActions(timerRunningMenu);
    m_timerRunning->setContextMenu(timerRunningMenu);
    connect(m_timerRunning, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        m_eventLoop.stop();

        if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick) [[unlikely]]
        {
            m_eventLoop.start();
            return;
        }

        if (!m_manager->isConnectedToInternet()) [[unlikely]]
        {
            showOfflineNotification();
            m_eventLoop.start();
            return;
        }

        if (m_user.getRunningTimeEntry())
            m_user.stopCurrentTimeEntry();
        else
        {
            auto project = defaultProject();
            m_user.startTimeEntry(project.id(), project.description());
        }

        updateTrayIcons();
        m_eventLoop.start();
    });

    if (Settings::instance()->useSeparateBreakTime())
    {
        auto runningJobMenu = new QMenu;
        connect(runningJobMenu->addAction(tr("Break")), &QAction::triggered, this, [this]() {
            if (!m_manager->isConnectedToInternet()) [[unlikely]]
                showOfflineNotification();

            if (auto e = m_user.getRunningTimeEntry(); e) [[likely]]
            {
                if (e->project().id() == Settings::instance()->breakTimeId())
                    return;

                m_user.stopCurrentTimeEntry();
            }
            m_user.startTimeEntry(Settings::instance()->breakTimeId());
            updateTrayIcons();
        });
        connect(runningJobMenu->addAction(tr("Resume")), &QAction::triggered, this, [this]() {
            if (!m_manager->isConnectedToInternet()) [[unlikely]]
                showOfflineNotification();

            if (auto e = m_user.getRunningTimeEntry(); e) [[likely]]
            {
                if (e->project().id() != Settings::instance()->breakTimeId())
                    return;

                m_user.stopCurrentTimeEntry();
            }
            auto project = defaultProject();
            m_user.startTimeEntry(project.id(), project.description());
            updateTrayIcons();
        });
        addMenuActions(runningJobMenu);
        m_runningJob->setContextMenu(runningJobMenu);
        connect(m_runningJob, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            m_eventLoop.stop();

            if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick) [[unlikely]]
            {
                m_eventLoop.start();
                return;
            }

            if (!m_manager->isConnectedToInternet()) [[unlikely]]
            {
                showOfflineNotification();
                m_eventLoop.start();
                return;
            }

            if (auto runningEntry = m_user.getRunningTimeEntry(); runningEntry)
            {
                if (runningEntry->project().id() == Settings::instance()->breakTimeId())
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

            updateTrayIcons();
            m_eventLoop.start();
        });
    }

    connect(m_manager, &ClockifyManager::internetConnectionChanged, this, [this](bool) { updateTrayIcons(); });

    connect(this, &TrayIcons::jobEnded, this, [this] {
        if (!Settings::instance()->showDurationNotifications())
            return;

        // This will either return the job that just ended, plus the job before, or the job that just started and the job
        // that just ended. Either way, we can get enough info to run the notifications.
        auto jobs = m_user.getTimeEntries(1, 2);
        if (jobs.isEmpty())
            return;
        auto job =
            std::find_if(jobs.begin(), jobs.end(), [this](const auto &j) { return j.id() == m_jobToBeNotified.id(); });
        if (job == jobs.end() || job->running().value_or(false))
            return;

        QTime duration{QTime::fromMSecsSinceStartOfDay(static_cast<int>(job->start().msecsTo(job->end())))};
        QString timeString{tr("%n minute(s)", nullptr, duration.minute())};
        if (duration.hour() > 0)
            timeString.prepend(tr("%n hour(s) and ", nullptr, duration.hour()));
        else
            timeString.append(tr(" and %n second(s)", nullptr, duration.second()));
        auto message =
            (Settings::instance()->useSeparateBreakTime() && job->project().id() == Settings::instance()->breakTimeId()) ?
                tr("You were on break for %1") :
                tr("You worked %2 on %1").arg(job->project().name());
        m_timerRunning->showMessage(tr("Job ended"), message.arg(timeString), QSystemTrayIcon::Information, 5000);

        updateQuickStartList();
    });
    connect(this, &TrayIcons::jobStarted, this, &TrayIcons::updateQuickStartList);

    updateTrayIcons();
}

void TrayIcons::setTimerState(TimerState state)
{
    // This is a hacky way of forcing the ratelimited icon, since otherwise it will generally get overridden
    if (m_ratelimited)
        state = TimerState::Ratelimited;

    if (state == m_timerState || state == TimerState::StateUnset)
        return;

    switch (state)
    {
    case TimerState::Running:
        m_timerRunning->setIcon(
            QIcon{m_runningJob ? QStringLiteral(":/icons/greenpower.png") : QStringLiteral(":/icons/greenlight.png")});
        if (m_runningJob)
        {
            m_timerRunning->setToolTip(tr("%1 is running").arg(m_manager->serviceName()));
            m_runningJob->setToolTip(tr("Working on %1").arg(m_currentRunningJob.project().name()));
            m_runningJob->setIcon(QIcon{":/icons/greenlight.png"});
        }
        else
            m_timerRunning->setToolTip(tr("Working on %1").arg(m_currentRunningJob.project().name()));

        if (m_timerState == TimerState::OnBreak)
            emit jobEnded();
        if (m_timerState != TimerState::Running)
            emit jobStarted();
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
        if (m_timerState != TimerState::OnBreak)
            emit jobStarted();
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

        if (m_timerState == TimerState::Running || m_timerState == TimerState::OnBreak)
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
    case TimerState::Ratelimited:
        m_timerRunning->setToolTip(tr("You have been ratelimited"));
        m_timerRunning->setIcon(
            QIcon{m_runningJob ? QStringLiteral(":/icons/graypower.png") : QStringLiteral(":/icons/graylight.png")});
        if (m_runningJob)
        {
            m_runningJob->setToolTip(tr("You have been ratelimited"));
            m_runningJob->setIcon(QIcon{":/icons/graylight.png"});
        }
        break;
    default:
        break;
    }

    m_timerState = state;
    emit timerStateChanged();
}

void TrayIcons::updateQuickStartList()
{
    if (!m_quickStartMenu)
        m_quickStartMenu = new QMenu{tr("Switch to")};
    m_quickStartMenu->clear();

    auto addMenuEntry = [this](const Project &project) {
        connect(m_quickStartMenu->addAction(project.name()), &QAction::triggered, this, [projectId = project.id(), this] {
            m_eventLoop.stop();
            if (!m_manager->isConnectedToInternet()) [[unlikely]]
            {
                showOfflineNotification();
                m_eventLoop.start();
                return;
            }

            bool hadRunningJob{false};
            QDateTime now{m_manager->currentDateTime()};
            if (auto runningEntry = m_user.getRunningTimeEntry(); runningEntry)
            {
                hadRunningJob = true;
                if (runningEntry->project().id() == projectId)
                {
                    m_eventLoop.start();
                    return;
                }
                now = m_user.stopCurrentTimeEntry();
            }
            m_user.startTimeEntry(projectId, defaultProject().description(), now, true);
            if (hadRunningJob)
                emit jobEnded();
            emit jobStarted();
            m_eventLoop.start();
        });
    };

    switch (Settings::instance()->quickStartProjectsLoading())
    {
    case Settings::QuickStartProjectOptions::AllProjects:
    {
        for (const auto &project : m_manager->projects())
            addMenuEntry(project);
        break;
    }
    case Settings::QuickStartProjectOptions::RecentProjects:
    {
        auto entries = m_user.getTimeEntries(m_manager->paginationStartsAt(), 25);
        QStringList addedIds;
        for (const auto &entry : entries)
        {
            if (addedIds.size() >= 10)
                break;
            if (Settings::instance()->useSeparateBreakTime() && entry.project().id() == Settings::instance()->breakTimeId())
                [[unlikely]]
                continue;
            if (addedIds.contains(entry.project().id()))
                continue;
            addedIds.push_back(entry.project().id());
            addMenuEntry(entry.project());
        }
        break;
    }
    default:
        break;
    }
}

void TrayIcons::showOfflineNotification()
{
    m_timerRunning->showMessage(tr("Internet connection lost"),
                                tr("The request could not be completed because the internet connection is "
                                   "down."));
}

void TrayIcons::checkForFinishedWeek()
{
    // we can't show a bubble if the icon isn't visible
    if (Settings::instance()->alertOnTimeUp() && m_timeUpWarning != TimeUpWarning::Done && m_timerRunning->isVisible())
    {
        auto now = QDateTime::currentDateTime();
        auto entries = m_user.getTimeEntries(std::nullopt,
                                             std::nullopt,
                                             now.addDays(-(now.date().dayOfWeek() % 7)),
                                             now.addDays(6 - (now.date().dayOfWeek() % 7)));
        double msecsThisWeek = std::accumulate(entries.begin(), entries.end(), 0, [](auto a, auto b) {
            if (b.end().isNull() && b.running().value_or(true))
                b.setEnd(QDateTime::currentDateTime());
            return a + b.start().msecsTo(b.end());
        });
        double hoursThisWeek = msecsThisWeek / (1000 * 60 * 60);
        if (hoursThisWeek >= Settings::instance()->weekHours())
        {
            m_timerRunning->showMessage(
                tr("Your week is done"),
                tr("You have now worked %n hour(s) this week!", nullptr, Settings::instance()->weekHours()));
            m_timeUpWarning = TimeUpWarning::Done;
        }
        // warn 1 hour before time is up
        else if (hoursThisWeek >= Settings::instance()->weekHours() - 1 && m_timeUpWarning != TimeUpWarning::AlmostDone)
        {
            m_timerRunning->showMessage(tr("You're almost done"),
                                        tr("You have less than an hour to go to complete your work this week!"));
            m_timeUpWarning = TimeUpWarning::AlmostDone;
            // fifteen-second delay to make sure we don't get ahead of the time service by any chance
            QTimer::singleShot(msecsThisWeek + 15000, this, &TrayIcons::checkForFinishedWeek);
        }
    }
}

template<TimeManager Manager> void TrayIcons::initializeManager()
{
    m_manager = new Manager{Settings::instance()->apiKey().toUtf8()};
}
