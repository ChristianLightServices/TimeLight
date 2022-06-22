#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QTimer>

class Settings : public QObject
{
    Q_OBJECT

public:
    enum class QuickStartProjectOptions
    {
        AllProjects,
        RecentProjects,

        Undefined,
    };
    Q_ENUM(QuickStartProjectOptions)

    const QString &timeService() const { return m_timeService; }
    const QString &apiKey() const { return m_apiKey; }
    const QString &breakTimeId() const { return m_breakTimeId; }
    const QString &description() const { return m_description; }
    const QString &projectId() const { return m_projectId; }
    const QString &workspaceId() const { return m_workspaceId; }
    bool useSeparateBreakTime() const { return m_useSeparateBreakTime; }
    bool disableDescription() const { return m_disableDescription; }
    bool useLastDescription() const { return m_useLastDescription; }
    bool useLastProject() const { return m_useLastProject; }
    bool showDurationNotifications() const { return m_showDurationNotifications; }
    int eventLoopInterval() const { return m_eventLoopInterval; }
    QuickStartProjectOptions quickStartProjectsLoading() const { return m_quickStartProjectsLoading; }
    bool alertOnTimeUp() const { return m_alertOnTimeUp; }
    double weekHours() const { return m_weekHours; }
    bool developerMode() const { return m_developerMode; }

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
    void setDisableDescription(const bool disable);
    void setUseLastDescription(const bool use);
    void setUseLastProject(const bool use);
    void setShowDurationNotifications(const bool show);
    void setEventLoopInterval(const int interval);
    void setQuickStartProjectsLoading(const Settings::QuickStartProjectOptions &option);
    void setAlertOnTimeUp(const bool alert);
    void setWeekHours(const double hours);
    void setDeveloperMode(const bool state);

signals:
    void timeServiceChanged();
    void apiKeyChanged();
    void breakTimeIdChanged();
    void descriptionChanged();
    void projectIdChanged();
    void workspaceIdChanged();
    void useSeparateBreakTimeChanged();
    void disableDescriptionChanged();
    void useLastDescriptionChanged();
    void useLastProjectChanged();
    void showDurationNotificationsChanged();
    void eventLoopIntervalChanged();
    void quickStartProjectsLoadingChanged();
    void alertOnTimeUpChanged();
    void weekHoursChanged();
    void developerModeChanged();

private:
    explicit Settings(QObject *parent = nullptr);
    ~Settings() override;

    void load();
    void save();

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
    bool m_disableDescription;
    bool m_useLastDescription;
    bool m_useLastProject;
    bool m_showDurationNotifications;
    bool m_alertOnTimeUp;
    QuickStartProjectOptions m_quickStartProjectsLoading;

    double m_weekHours;
    int m_eventLoopInterval;

    bool m_developerMode;
};

#endif // SETTINGS_H
