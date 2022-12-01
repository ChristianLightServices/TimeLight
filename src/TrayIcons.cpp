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

#include "DailyOverviewDialog.h"
#include "DeveloperTools.h"
#include "Logger.h"
#include "ModifyJobDialog.h"
#include "Project.h"
#include "Settings.h"
#include "SettingsDialog.h"
#include "SetupFlow.h"
#include "User.h"
#include "Utils.h"
#include "version.h"

namespace logs = TimeLight::logs;

TrayIcons::TrayIcons(QObject *parent)
    : QObject{parent},
      m_trayIcon{new QSystemTrayIcon},
      m_recentEntries{new QList<TimeEntry>}
{
    if (Settings::instance()->useTeamsIntegration())
        setUpTeams();

    auto fixApiKey = [&] {
        while (!m_manager->isValid())
        {
            logs::app()->debug("Fixing API key");
            QMessageBox::warning(nullptr, tr("API key"), tr("The API key seems to be incorrect or invalid."));
            SetupFlow flow{this};
            flow.resetApiKeyStage();
            if (auto r = flow.runApiKeyStage(); r == SetupFlow::Result::Canceled)
                return false;
        }

        return true;
    };

    SetupFlow flow{this};

    auto r = flow.runNextStage();
    while (!flow.done())
    {
        switch (r)
        {
        case SetupFlow::Result::Valid:
            if (flow.stage() == SetupFlow::Stage::ApiKey)
            {
                if (!fixApiKey())
                {
                    flow.resetPreviousStage();
                    r = flow.runPreviousStage();
                    break;
                }
            }
            r = flow.runNextStage();
            break;
        case SetupFlow::Result::Invalid:
            r = flow.rerunCurrentStage();
            break;
        case SetupFlow::Result::Canceled:
            if (flow.stage() == SetupFlow::Stage::TimeService)
            {
                m_valid = false;
                return;
            }
            else
            {
                flow.resetPreviousStage();
                r = flow.runPreviousStage();
            }
            break;
        default:
            break;
        }
    }

    connect(m_manager.data(), &AbstractTimeServiceManager::invalidated, this, [this, fixApiKey] {
        if (!fixApiKey())
        {
            m_valid = false;
            return;
        }
    });

    m_user = m_manager->getApiKeyOwner();
    if (!m_user.isValid()) [[unlikely]]
    {
        logs::app()->error("Invalid user!");
        QMessageBox::critical(nullptr, tr("Fatal error"), tr("Could not load user!"));
        m_valid = false;
        return;
    }

    if (Settings::instance()->useSeparateBreakTime() && !Settings::instance()->middleClickForBreak())
        m_breakIcon = new QSystemTrayIcon;

    m_nextPage = m_manager->paginationStartsAt();
    loadMoreEntries();

    setUpTrayIcon();

    m_eventLoop.setInterval(Settings::instance()->eventLoopInterval());
    m_eventLoop.setSingleShot(false);
    m_eventLoop.callOnTimeout(this, &TrayIcons::updateTrayIcons);
    m_eventLoop.start();

    m_updateRunningEntryTooltipTimer.setInterval(1000);
    m_updateRunningEntryTooltipTimer.setSingleShot(false);
    m_updateRunningEntryTooltipTimer.callOnTimeout(this, &TrayIcons::updateRunningEntryTooltip);

    // check for completed work time every hour
    m_alertOnTimeUpTimer.setInterval(1000 * 60 * 60);
    m_alertOnTimeUpTimer.setSingleShot(false);
    m_alertOnTimeUpTimer.callOnTimeout(this, &TrayIcons::checkForFinishedWeek);
    m_alertOnTimeUpTimer.start();

    connect(Settings::instance(), &Settings::eventLoopIntervalChanged, this, [this] {
        logs::app()->trace("Changing event loop interval to {}", Settings::instance()->eventLoopInterval());
        m_eventLoop.setInterval(Settings::instance()->eventLoopInterval());
    });

    connect(m_manager.data(), &AbstractTimeServiceManager::ratelimited, this, [this](bool ratelimited) {
        m_ratelimited = ratelimited;
        if (m_ratelimited)
        {
            logs::network()->trace("ratelimited");
            // Use exponential backoff with a max interval of 10 minutes
            m_eventLoop.setInterval(std::min(std::pow(m_eventLoop.interval(), 1.25), 10 * 60 * 1000.));
            setTimerState(TimerState::Ratelimited);
        }
        else
        {
            logs::network()->trace("unratelimited");
            m_eventLoop.setInterval(Settings::instance()->eventLoopInterval());
        }
    });

    connect(this, &TrayIcons::jobStarted, this, &TrayIcons::updateTrayIcons, Qt::QueuedConnection);

    connect(Settings::instance(), &Settings::useTeamsIntegrationChanged, this, [this] {
        if (Settings::instance()->useTeamsIntegration())
            setUpTeams();
        else
            m_teamsClient.clear();
    });
}

TrayIcons::~TrayIcons()
{
    delete m_trayIcon;
    if (m_breakIcon)
        delete m_breakIcon;
}

void TrayIcons::show()
{
    m_trayIcon->show();
    if (m_breakIcon)
        m_breakIcon->show();
    checkForFinishedWeek();
}

Project TrayIcons::defaultProject()
{
    Project project;
    int pageNum{m_manager->paginationStartsAt()};
    bool itemsLeftToLoad{true};

    if (!Settings::instance()->useLastProject())
    {
        auto projects = qAsConst(m_manager->projects());
        if (auto p = std::find_if(projects.begin(),
                                  projects.end(),
                                  [](const Project &p) { return p.id() == Settings::instance()->projectId(); });
            p == projects.end())
        {
            logs::app()->warn("Attempted to use invalid project ID for default project");
            project = projects.first();
        }
        else
            project = *p;
    }
    else
    {
        do
        {
            auto newEntries = m_user.getTimeEntries(pageNum++);
            if (newEntries.empty())
                itemsLeftToLoad = false;
            else
                m_recentEntries->append(newEntries);

            for (const auto &entry : newEntries)
            {
                if (entry.project().id().isEmpty()) [[unlikely]]
                {
                    logs::network()->error("Getting project id failed");
                    continue; // no project id to see here, move along
                }
                else if (!Settings::instance()->useSeparateBreakTime() ||
                         entry.project().id() != Settings::instance()->breakTimeId())
                {
                    project = entry.project();
                    break;
                }
            }
            if (!m_manager->supportedPagination().testFlag(AbstractTimeServiceManager::Pagination::TimeEntries))
                break;
        } while (itemsLeftToLoad && project.id().isEmpty());

        // when all else fails, use the first extant project
        if (project.id().isEmpty()) [[unlikely]]
        {
            logs::app()->info("Could not load a suitable project from past entries");
            project = m_manager->projects().first();
        }
    }

    if (Settings::instance()->disableDescription())
        project.setDescription({});
    else if (!Settings::instance()->useLastDescription())
        project.setDescription(Settings::instance()->description());
    else if (!Settings::instance()->useLastProject()) // if we are using the last project, we have the last description
    {
        bool descriptionLoaded{false};

        do
        {
            for (const auto &entry : *m_recentEntries)
            {
                if (!Settings::instance()->useSeparateBreakTime() ||
                    entry.project().id() != Settings::instance()->breakTimeId())
                {
                    project.setDescription(entry.project().description());
                    descriptionLoaded = true;
                    break;
                }
            }
            if (!m_manager->supportedPagination().testFlag(AbstractTimeServiceManager::Pagination::TimeEntries))
                break;

            if (auto entries = m_user.getTimeEntries(++pageNum); entries.empty())
                itemsLeftToLoad = false;
            else
                m_recentEntries->append(entries);
        } while (itemsLeftToLoad && !descriptionLoaded);

        if (!descriptionLoaded) [[unlikely]]
        {
            logs::app()->info("Could not load a suitable description from past entries");
            project.setDescription(m_manager->projects().first().description());
        }
    }

    // update the recents
//    if (entries.size() >= 25)
//    {
//        m_recents->clear();
//        for (int i = 0; i < 25; ++i)
//            m_recents->append(entries[i]);
//    }

    return project;
}

void TrayIcons::updateTrayIcons()
{
    if (!m_manager->isConnectedToInternet()) [[unlikely]]
    {
        setTimerState(TimerState::Offline);
        // make requests to see if the internet came back up
        m_user.getRunningTimeEntry();
    }
    else if (auto runningEntry = m_user.getRunningTimeEntry(); runningEntry)
    {
        try
        {
            m_currentRunningJob = *runningEntry;
            if (m_recentEntries->size() > 0 && *runningEntry != m_recentEntries->first())
                m_recentEntries->prepend(*runningEntry); // new entry
            else
                m_recentEntries->replace(0, *runningEntry); // current entry may have changed

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
            logs::network()->error("Could not load running time entry: {}", ex.what());
        }
        catch (...)
        {
            logs::network()->error("Could not load running time entry");
        }
    }
    else
        setTimerState(TimerState::NotRunning);
}

void TrayIcons::updateRunningEntryTooltip()
{
    QString tooltip = m_runningEntryTooltipBase;
    if (m_currentRunningJob)
    {
        auto [h, m, s] =
            TimeLight::msecsToHoursMinutesSeconds(m_currentRunningJob->start().msecsTo(QDateTime::currentDateTime()));
        tooltip += QStringLiteral(" (%1:%2:%3)").arg(h).arg(m, 2, 10, QLatin1Char{'0'}).arg(s, 2, 10, QLatin1Char{'0'});
    }
    (m_breakIcon ? m_breakIcon : m_trayIcon)->setToolTip(tooltip);
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

    auto dialog = new QDialog;
    auto layout = new QVBoxLayout{dialog};

    auto infoLabel = new QLabel{aboutText, dialog};
    infoLabel->setTextFormat(Qt::MarkdownText);
    infoLabel->setWordWrap(true);
    infoLabel->setOpenExternalLinks(true);
    layout->addWidget(infoLabel);

    auto bb = new QDialogButtonBox{QDialogButtonBox::Ok, dialog};
    connect(
        bb->addButton(tr("About Qt"), QDialogButtonBox::ActionRole), &QPushButton::clicked, qApp, &QApplication::aboutQt);
    connect(bb->addButton(tr("Show license"), QDialogButtonBox::ActionRole), &QPushButton::clicked, this, [this, dialog] {
        showLicenseDialog(dialog);
    });
    connect(bb->addButton(tr("Show source code"), QDialogButtonBox::ActionRole), &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl{QStringLiteral("https://github.com/ChristianLightServices/TimeLight")});
    });
    connect(bb, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);

    layout->addWidget(bb);

    dialog->setLayout(layout);
    dialog->setWindowTitle(tr("About ") + qApp->applicationName());
    dialog->resize(500, dialog->heightForWidth(500));
    dialog->move(dialog->screen()->geometry().width() / 2 - dialog->width() / 2,
                 dialog->screen()->geometry().height() / 2 - dialog->height() / 2);
    dialog->exec();
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

void TrayIcons::addStandardMenuActions(QMenu *menu)
{
    menu->addMenu(m_quickStartMenu.data());
    auto modifyJob = menu->addAction(tr("Modify current job"));
    connect(modifyJob, &QAction::triggered, this, [this] {
        if (auto e = m_user.getRunningTimeEntry(); e)
        {
            auto dialog = new ModifyJobDialog{m_manager, *e, m_recentEntries};
            dialog->show();
            connect(dialog, &QDialog::finished, this, [this, dialog](int result) {
                if (result == QDialog::Accepted)
                {
                    m_user.modifyTimeEntry(std::move(dialog->entry()), true);
                    updateTrayIcons();
                    updateQuickStartList();
                }
                dialog->deleteLater();
            });
        }
    });
    auto cancel = menu->addAction(tr("Cancel current job"));
    connect(cancel, &QAction::triggered, this, [this] {
        if (m_currentRunningJob)
        {
            if (QMessageBox::question(nullptr, tr("Cancel job"), tr("Are you sure you want to cancel the current job?")) ==
                QMessageBox::Yes)
            {
                m_user.deleteTimeEntry(*m_currentRunningJob, true);
                updateTrayIcons();
            }
        }
    });
    modifyJob->setDisabled(true);
    cancel->setDisabled(true);
    connect(this, &TrayIcons::timerStateChanged, this, [this, modifyJob, cancel] {
        modifyJob->setEnabled(m_timerState == TimerState::Running || m_timerState == TimerState::OnBreak);
        cancel->setEnabled(m_timerState == TimerState::Running || m_timerState == TimerState::OnBreak);
    });

    connect(menu->addAction(tr("Daily time report")), &QAction::triggered, this, [this] {
        DailyOverviewDialog d{m_manager, &m_user};
        d.exec();
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

    connect(menu->addAction(tr("Open the %1 webpage").arg(m_manager->serviceName())), &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(m_manager->timeTrackerWebpageUrl());
    });
    connect(menu->addAction(tr("About")), &QAction::triggered, this, &TrayIcons::showAboutDialog);
    connect(menu->addAction(tr("Quit")), &QAction::triggered, qApp, &QApplication::quit);
}

QAction *TrayIcons::createBreakResumeAction()
{
    auto breakResumeAction = new QAction{tr("Break")};
    connect(this, &TrayIcons::timerStateChanged, breakResumeAction, [this, breakResumeAction] {
        breakResumeAction->setEnabled(true);
        if (m_timerState == TimerState::Running || m_timerState == TimerState::NotRunning)
            breakResumeAction->setText(tr("Break"));
        else if (m_timerState == TimerState::OnBreak)
            breakResumeAction->setText(tr("Resume"));
        else
            breakResumeAction->setDisabled(true);
    });
    connect(breakResumeAction, &QAction::triggered, this, [this] {
        if (!m_manager->isConnectedToInternet()) [[unlikely]]
            showOfflineNotification();

        if (m_timerState == TimerState::NotRunning || m_timerState == TimerState::Running)
        {
            if (m_timerState == TimerState::Running)
                m_user.stopCurrentTimeEntry();
            m_user.startTimeEntry(Project{Settings::instance()->breakTimeId(), {}});
        }
        else if (m_timerState == TimerState::OnBreak)
        {
            m_user.stopCurrentTimeEntry();
            auto project = defaultProject();
            m_user.startTimeEntry(project);
        }

        updateTrayIcons();
    });

    return breakResumeAction;
}

void TrayIcons::setUpTrayIcon()
{
    logs::app()->trace("Setting up tray icon");

    updateQuickStartList();

    auto trayIconMenu = new QMenu;
    auto startStopAction = trayIconMenu->addAction(tr("Start"));
    connect(this, &TrayIcons::timerStateChanged, startStopAction, [this, startStopAction] {
        startStopAction->setEnabled(true);
        if (m_timerState == TimerState::NotRunning)
            startStopAction->setText(tr("Start"));
        else if (m_timerState == TimerState::Running || m_timerState == TimerState::OnBreak)
            startStopAction->setText(tr("Stop"));
        else
            startStopAction->setDisabled(true);
    });
    connect(startStopAction, &QAction::triggered, this, [this] {
        if (!m_manager->isConnectedToInternet()) [[unlikely]]
            showOfflineNotification();

        if (m_timerState == TimerState::NotRunning)
        {
            auto project = defaultProject();
            m_user.startTimeEntry(project);
        }
        else if (m_timerState == TimerState::Running || m_timerState == TimerState::OnBreak)
            m_user.stopCurrentTimeEntry();

        updateTrayIcons();
    });
    auto breakResumeAction = createBreakResumeAction();
    trayIconMenu->addAction(breakResumeAction);
    if (!(Settings::instance()->useSeparateBreakTime() && Settings::instance()->middleClickForBreak()))
        breakResumeAction->setVisible(false);
    addStandardMenuActions(trayIconMenu);
    m_trayIcon->setContextMenu(trayIconMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        m_eventLoop.stop();

        if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick &&
            !(Settings::instance()->useSeparateBreakTime() && Settings::instance()->middleClickForBreak() &&
              reason == QSystemTrayIcon::MiddleClick)) [[unlikely]]
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

        if (reason == QSystemTrayIcon::MiddleClick)
        {
            if (m_currentRunningJob)
            {
                if (m_currentRunningJob->project().id() == Settings::instance()->breakTimeId())
                {
                    auto time = m_user.stopCurrentTimeEntry();
                    auto project = defaultProject();
                    m_user.startTimeEntry(project, time);
                }
                else
                {
                    auto time = m_user.stopCurrentTimeEntry();
                    m_user.startTimeEntry(Project{Settings::instance()->breakTimeId(), {}}, time);
                }
            }
            else
            {
                auto project = defaultProject();
                m_user.startTimeEntry(Project{Settings::instance()->breakTimeId(), {}});
            }
        }
        else if (m_currentRunningJob)
            m_user.stopCurrentTimeEntry();
        else
        {
            auto project = defaultProject();
            m_user.startTimeEntry(project);
        }

        updateTrayIcons();
        m_eventLoop.start();
    });

    if (m_breakIcon)
        setUpBreakIcon();

    connect(m_manager.data(),
            &AbstractTimeServiceManager::internetConnectionChanged,
            this,
            &TrayIcons::updateTrayIcons,
            Qt::QueuedConnection);

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
        m_trayIcon->showMessage(tr("Job ended"), message.arg(timeString), QSystemTrayIcon::Information, 5000);

        updateQuickStartList();
    });
    connect(this, &TrayIcons::jobStarted, this, &TrayIcons::updateQuickStartList, Qt::QueuedConnection);

    auto showOrHideBreakButton = [this, breakResumeAction] {
        if (Settings::instance()->useSeparateBreakTime() && !Settings::instance()->middleClickForBreak())
        {
            logs::app()->trace("Showing break icon");
            if (m_breakIcon == nullptr)
                m_breakIcon = new QSystemTrayIcon{this};
            setUpBreakIcon();
            breakResumeAction->setVisible(false);
            m_breakIcon->show();
        }
        else if (m_breakIcon)
        {
            logs::app()->trace("Hiding break icon");
            m_breakIcon->deleteLater();
            m_breakIcon = nullptr;
            if (Settings::instance()->useSeparateBreakTime())
                breakResumeAction->setVisible(true);
        }
        updateIconsAndTooltips();
        updateQuickStartList();
    };
    connect(Settings::instance(), &Settings::useSeparateBreakTimeChanged, this, showOrHideBreakButton);
    connect(Settings::instance(), &Settings::middleClickForBreakChanged, this, showOrHideBreakButton);

    updateTrayIcons();
}

void TrayIcons::setUpBreakIcon()
{
    logs::app()->trace("Setting up break icon");

    auto breakIconMenu = new QMenu;
    breakIconMenu->addAction(createBreakResumeAction());
    addStandardMenuActions(breakIconMenu);
    m_breakIcon->setContextMenu(breakIconMenu);
    connect(m_breakIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
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

        if (m_currentRunningJob)
        {
            if (m_currentRunningJob->project().id() == Settings::instance()->breakTimeId())
            {
                auto time = m_user.stopCurrentTimeEntry();
                auto project = defaultProject();
                m_user.startTimeEntry(project, time);
            }
            else
            {
                auto time = m_user.stopCurrentTimeEntry();
                m_user.startTimeEntry(Project{Settings::instance()->breakTimeId(), {}}, time);
            }
        }
        else
        {
            auto project = defaultProject();
            m_user.startTimeEntry(project);
        }

        updateTrayIcons();
        m_eventLoop.start();
    });
}

void TrayIcons::setUpTeams()
{
    m_teamsClient.reset(new TeamsClient{TimeLight::teamsAppId, 6942, this});

    connect(m_teamsClient.data(), &TeamsClient::accessTokenChanged, Settings::instance(), [this] {
        Settings::instance()->setGraphAccessToken(m_teamsClient->accessToken());
    });
    connect(m_teamsClient.data(), &TeamsClient::refreshTokenChanged, Settings::instance(), [this] {
        Settings::instance()->setGraphRefreshToken(m_teamsClient->refreshToken());
    });

    m_teamsClient->setAccessToken(Settings::instance()->graphAccessToken());
    m_teamsClient->setRefreshToken(Settings::instance()->graphRefreshToken());
    m_teamsClient->authenticate();
}

void TrayIcons::setTimerState(TimerState state)
{
    // This is a hacky way of forcing the ratelimited icon, since otherwise it will generally get overridden
    if (m_ratelimited)
        state = TimerState::Ratelimited;

    if (state == m_timerState)
        return;

    switch (state)
    {
    case TimerState::Running:
    {
        if (m_timerState == TimerState::OnBreak)
            emit jobEnded();
        if (m_timerState != TimerState::Running)
            emit jobStarted();

        if (Settings::instance()->useTeamsIntegration() && m_teamsClient->authenticated())
            m_teamsClient->setPresence(Settings::instance()->presenceWhileWorking());
        break;
    }
    case TimerState::OnBreak:
        if (m_timerState == TimerState::Running)
            emit jobEnded();
        if (m_timerState != TimerState::OnBreak)
            emit jobStarted();

        if (Settings::instance()->useTeamsIntegration() && m_teamsClient->authenticated())
            m_teamsClient->setPresence(Settings::instance()->presenceWhileOnBreak());
        break;
    case TimerState::NotRunning:
        if (m_timerState == TimerState::Running || m_timerState == TimerState::OnBreak)
            emit jobEnded();

        if (Settings::instance()->useTeamsIntegration() && m_teamsClient->authenticated())
            m_teamsClient->setPresence(Settings::instance()->presenceWhileNotWorking());
        break;
    default:
        break;
    }

    m_timerState = state;
    updateIconsAndTooltips();
    emit timerStateChanged();
}

void TrayIcons::updateIconsAndTooltips()
{
    switch (m_timerState)
    {
    case TimerState::Running:
    {
        m_trayIcon->setIcon(
            QIcon{m_breakIcon ? QStringLiteral(":/icons/greenpower.png") : QStringLiteral(":/icons/greenlight.png")});
        if (m_breakIcon)
        {
            m_trayIcon->setToolTip(tr("%1 is running").arg(m_manager->serviceName()));
            m_breakIcon->setIcon(QIcon{":/icons/greenlight.png"});
        }

        m_runningEntryTooltipBase = tr("Working on %1").arg(m_currentRunningJob->project().name());
        updateRunningEntryTooltip();
        m_updateRunningEntryTooltipTimer.start();
        break;
    }
    case TimerState::OnBreak:
        if (m_breakIcon)
        {
            m_trayIcon->setToolTip(tr("%1 is running").arg(m_manager->serviceName()));
            m_trayIcon->setIcon(QIcon{QStringLiteral(":/icons/greenpower.png")});
            m_breakIcon->setIcon(QIcon{":/icons/yellowlight.png"});
        }
        else
            m_trayIcon->setIcon(QIcon{":/icons/yellowlight.png"});

        m_runningEntryTooltipBase = tr("You are on break");
        updateRunningEntryTooltip();
        m_updateRunningEntryTooltipTimer.start();
        break;
    case TimerState::NotRunning:
        m_updateRunningEntryTooltipTimer.stop();

        m_trayIcon->setToolTip(tr("%1 is not running").arg(m_manager->serviceName()));
        m_trayIcon->setIcon(
            QIcon{m_breakIcon ? QStringLiteral(":/icons/redpower.png") : QStringLiteral(":/icons/redlight.png")});
        if (m_breakIcon)
        {
            m_breakIcon->setToolTip(tr("You are not working"));
            m_breakIcon->setIcon(QIcon{":/icons/redlight.png"});
        }
        break;
    case TimerState::Offline:
        m_updateRunningEntryTooltipTimer.stop();

        m_trayIcon->setToolTip(tr("You are offline"));
        m_trayIcon->setIcon(
            QIcon{m_breakIcon ? QStringLiteral(":/icons/graypower.png") : QStringLiteral(":/icons/graylight.png")});
        if (m_breakIcon)
        {
            m_breakIcon->setToolTip(tr("You are offline"));
            m_breakIcon->setIcon(QIcon{":/icons/graylight.png"});
        }
        break;
    case TimerState::Ratelimited:
        m_updateRunningEntryTooltipTimer.stop();

        m_trayIcon->setToolTip(tr("You have been ratelimited"));
        m_trayIcon->setIcon(
            QIcon{m_breakIcon ? QStringLiteral(":/icons/graypower.png") : QStringLiteral(":/icons/graylight.png")});
        if (m_breakIcon)
        {
            m_breakIcon->setToolTip(tr("You have been ratelimited"));
            m_breakIcon->setIcon(QIcon{":/icons/graylight.png"});
        }
        break;
    default:
        break;
    }
}

void TrayIcons::updateQuickStartList()
{
    if (m_updatingQuickStartList)
        return;
    m_updatingQuickStartList = true;

    if (!m_quickStartMenu)
        m_quickStartMenu.reset(new QMenu{tr("Switch to")});
    if (!m_quickStartAllProjects)
        m_quickStartAllProjects.reset(new QMenu{tr("All projects")});

    m_quickStartMenu->clear();
    m_quickStartAllProjects->clear();

    auto addMenuEntry = [this](QSharedPointer<QMenu> menu, const Project &project) {
        auto name{project.name()};
        if (!project.description().isEmpty())
            name.append(" | ").append(project.description());
        connect(menu->addAction(name), &QAction::triggered, this, [project, this] {
            m_eventLoop.stop();
            if (!m_manager->isConnectedToInternet()) [[unlikely]]
            {
                showOfflineNotification();
                m_eventLoop.start();
                return;
            }

            QDateTime now{m_manager->currentDateTime()};
            if (m_currentRunningJob)
            {
                if (Settings::instance()->preventSplittingEntries() && m_currentRunningJob->project().id() == project.id())
                {
                    m_eventLoop.start();
                    return;
                }
                now = m_user.stopCurrentTimeEntry();
            }
            m_user.startTimeEntry(project, now, true);
            m_eventLoop.start();
        });
    };

    std::optional<Project> addBreakTime;
    QList<Project> recentProjects;
    for (auto &entry : *m_recentEntries)
    {
        if (recentProjects.size() >= 10)
            break;
        if (Settings::instance()->useSeparateBreakTime() && entry.project().id() == Settings::instance()->breakTimeId())
            [[unlikely]]
        {
            addBreakTime = entry.project();
            continue;
        }
        if (!Settings::instance()->splitByDescription())
        {
            auto p = entry.project();
            p.setDescription({});
            entry.setProject(p);
        }
        if (std::find_if(recentProjects.begin(), recentProjects.end(), [&entry](const auto &a) {
                return a == entry.project();
            }) != recentProjects.end())
            continue;
        recentProjects.push_back(entry.project());
    }
    for (const auto &entry : recentProjects)
        addMenuEntry(m_quickStartMenu, entry);
    for (const auto &project : m_manager->projects())
        addMenuEntry(m_quickStartAllProjects, project);
    // the break time project always comes last
    if (addBreakTime.has_value())
    {
        m_quickStartMenu->addSeparator();
        addMenuEntry(m_quickStartMenu, *addBreakTime);
    }

    m_quickStartMenu->addSeparator();
    m_quickStartMenu->addMenu(m_quickStartAllProjects.data());

    m_updatingQuickStartList = false;
}

void TrayIcons::showOfflineNotification()
{
    m_trayIcon->showMessage(tr("Internet connection lost"),
                            tr("The request could not be completed because the internet connection is "
                               "down."));
}

void TrayIcons::checkForFinishedWeek()
{
    // we can't show a bubble if the icon isn't visible
    if (Settings::instance()->alertOnTimeUp() && m_timeUpWarning != TimeUpWarning::Done && m_trayIcon->isVisible())
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
        double hoursThisWeek = std::get<0>(TimeLight::msecsToHoursMinutesSeconds(msecsThisWeek));
        if (hoursThisWeek >= Settings::instance()->weekHours())
        {
            m_trayIcon->showMessage(
                tr("Your week is done"),
                tr("You have now worked %n hour(s) this week!", nullptr, Settings::instance()->weekHours()));
            m_timeUpWarning = TimeUpWarning::Done;
        }
        // warn 1 hour before time is up
        else if (hoursThisWeek >= Settings::instance()->weekHours() - 1 && m_timeUpWarning != TimeUpWarning::AlmostDone)
        {
            m_trayIcon->showMessage(tr("You're almost done"),
                                    tr("You have less than an hour to go to complete your work this week!"));
            m_timeUpWarning = TimeUpWarning::AlmostDone;
            // fifteen-second delay to make sure we don't get ahead of the time service by any chance
            QTimer::singleShot(msecsThisWeek + 15000, this, &TrayIcons::checkForFinishedWeek);
        }
    }
}

void TrayIcons::loadMoreEntries()
{
    auto newEntries = m_user.getTimeEntries(m_nextPage++);
    if (m_recentEntries->contains(newEntries.last()))
    {
        auto currentPos = m_nextPage;
        invalidateEntries();
        while (m_nextPage < currentPos)
            loadMoreEntries();
    }
    else
        m_recentEntries->append(newEntries);
}

void TrayIcons::invalidateEntries()
{
    m_recentEntries->clear();
    m_nextPage = m_manager->paginationStartsAt();
}
