#ifndef TRAYICONS_H
#define TRAYICONS_H

#include <QAction>
#include <QObject>
#include <QSystemTrayIcon>
#include <QTimer>

#include "AbstractTimeServiceManager.h"
#include "Logger.h"
#include "Project.h"
#include "Settings.h"
#include "TeamsClient.h"
#include "User.h"

class TrayIcons : public QObject
{
    Q_OBJECT

public:
    explicit TrayIcons(QObject *parent = nullptr);
    ~TrayIcons() override;

    void show();

    Project defaultProject();

    bool valid() const { return m_valid; }

signals:
    void timerStateChanged();
    void jobStarted();
    void jobEnded();

private slots:
    void updateTrayIcons();
    void updateRunningEntryTooltip();

    void showAboutDialog();
    void showLicenseDialog(QWidget *parent = nullptr);

private:
    enum class TimerState
    {
        Running,
        OnBreak,
        NotRunning,
        Offline,
        Ratelimited,
        StateUnset,
    };

    enum class TimeUpWarning
    {
        NotDone,
        AlmostDone,
        Done,
    };

    template<TimeManager Manager>
    void initializeManager()
    {
        m_manager.reset(new Manager{Settings::instance()->apiKey().toUtf8()});
        m_manager->setLogger(TimeLight::logs::network());
    }

    void addStandardMenuActions(QMenu *menu);
    QAction *createBreakResumeAction();
    void setUpTrayIcon();
    void setUpBreakIcon();
    void setUpTeams();
    void setTimerState(TimerState state);
    void updateIconsAndTooltips();
    void updateQuickStartList();
    void showOfflineNotification();
    void checkForFinishedWeek();

    QSharedPointer<AbstractTimeServiceManager> m_manager;
    QSharedPointer<TeamsClient> m_teamsClient;

    QSystemTrayIcon *m_trayIcon{nullptr};
    QSystemTrayIcon *m_breakIcon{nullptr};

    QSharedPointer<QMenu> m_quickStartMenu{nullptr};
    QSharedPointer<QMenu> m_quickStartAllProjects{nullptr};
    bool m_updatingQuickStartList{false};

    QString m_runningEntryTooltipBase;
    QTimer m_updateRunningEntryTooltipTimer;

    User m_user;
    TimerState m_timerState{TimerState::StateUnset};
    std::optional<TimeEntry> m_currentRunningJob;
    TimeEntry m_jobToBeNotified;

    QTimer m_eventLoop;
    QTimer m_alertOnTimeUpTimer;

    bool m_valid{true};
    bool m_ratelimited{false};

    TimeUpWarning m_timeUpWarning{TimeUpWarning::NotDone};

    QSharedPointer<QList<Project>> m_recents;

    friend class SetupFlow;
};

#endif // TRAYICONS_H
