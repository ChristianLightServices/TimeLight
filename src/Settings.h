#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QTimer>

#include "TeamsClient.h"

class Settings : public QObject
{
    Q_OBJECT

public:
    const QString &timeService() const { return m_timeService; }
    const QString &apiKey() const { return m_apiKey; }
    const QString &breakTimeId() const { return m_breakTimeId; }
    const QString &description() const { return m_description; }
    const QString &projectId() const { return m_projectId; }
    const QString &workspaceId() const { return m_workspaceId; }
    bool useSeparateBreakTime() const { return m_useSeparateBreakTime; }
    bool middleClickForBreak() const { return m_middleClickForBreak; }
    bool disableDescription() const { return m_disableDescription; }
    bool useLastDescription() const { return m_useLastDescription; }
    bool useLastProject() const { return m_useLastProject; }
    bool showDurationNotifications() const { return m_showDurationNotifications; }
    int eventLoopInterval() const { return m_eventLoopInterval; }
    bool alertOnTimeUp() const { return m_alertOnTimeUp; }
    double weekHours() const { return m_weekHours; }
    bool developerMode() const { return m_developerMode; }
    bool useTeamsIntegration() const { return m_useTeamsIntegration; }
    const QString &graphAccessToken() const { return m_graphAccessToken; }
    const QString &graphRefreshToken() const { return m_graphRefreshToken; }
    TeamsClient::Presence presenceWhileWorking() const { return m_presenceWhileWorking; }
    TeamsClient::Presence presenceWhileOnBreak() const { return m_presenceWhileOnBreak; }
    TeamsClient::Presence presenceWhileNotWorking() const { return m_presenceWhileNotWorking; }

    static void init();
    static Settings *instance() { return s_instance; }

public slots:
    void setTimeService(const QString &service);
    void setApiKey(const QString &key);
    void setBreakTimeId(const QString &id);
    void setDescription(const QString &description);
    void setProjectId(const QString &id);
    void setWorkspaceId(const QString &id);
    void setUseSeparateBreakTime(const bool use);
    void setMiddleClickForBreak(const bool use);
    void setDisableDescription(const bool disable);
    void setUseLastDescription(const bool use);
    void setUseLastProject(const bool use);
    void setShowDurationNotifications(const bool show);
    void setEventLoopInterval(const int interval);
    void setAlertOnTimeUp(const bool alert);
    void setWeekHours(const double hours);
    void setDeveloperMode(const bool state);
    void setUseTeamsIntegration(const bool state);
    void setGraphAccessToken(const QString &token);
    void setGraphRefreshToken(const QString &token);
    void setPresenceWhileWorking(const TeamsClient::Presence &presence);
    void setPresenceWhileOnBreak(const TeamsClient::Presence &presence);
    void setPresenceWhileNotWorking(const TeamsClient::Presence &presence);

signals:
    void timeServiceChanged();
    void apiKeyChanged();
    void breakTimeIdChanged();
    void descriptionChanged();
    void projectIdChanged();
    void workspaceIdChanged();
    void useSeparateBreakTimeChanged();
    void middleClickForBreakChanged();
    void disableDescriptionChanged();
    void useLastDescriptionChanged();
    void useLastProjectChanged();
    void showDurationNotificationsChanged();
    void eventLoopIntervalChanged();
    void alertOnTimeUpChanged();
    void weekHoursChanged();
    void developerModeChanged();
    void useTeamsIntegrationChanged();
    void graphAccessTokenChanged();
    void graphRefreshTokenChanged();
    void presenceWhileWorkingChanged();
    void presenceWhileOnBreakChanged();
    void presenceWhileNotWorkingChanged();

private:
    explicit Settings(QObject *parent = nullptr);
    ~Settings() override;

    void load();
    void save(bool async = true);

    QTimer m_saveTimer;
    bool m_settingsDirty{false};
    static inline Settings *s_instance;

    QString m_timeService;

    QString m_apiKey;
    QString m_breakTimeId;
    QString m_description;
    QString m_projectId;
    QString m_workspaceId;

    bool m_useSeparateBreakTime;
    bool m_middleClickForBreak;
    bool m_disableDescription;
    bool m_useLastDescription;
    bool m_useLastProject;
    bool m_showDurationNotifications;
    bool m_alertOnTimeUp;

    double m_weekHours;
    int m_eventLoopInterval;

    bool m_developerMode;

    bool m_useTeamsIntegration;
    QString m_graphAccessToken;
    QString m_graphRefreshToken;
    TeamsClient::Presence m_presenceWhileWorking, m_presenceWhileOnBreak, m_presenceWhileNotWorking;
};

#endif // SETTINGS_H
