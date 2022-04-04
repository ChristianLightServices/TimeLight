#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QTimer>

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
    bool disableDescription() const { return m_disableDescription; }
    bool useLastDescription() const { return m_useLastDescription; }
    bool useLastProject() const { return m_useLastProject; }
    bool showDurationNotifications() const { return m_showDurationNotifications; }
    int eventLoopInterval() const { return m_eventLoopInterval; }

    static void init();
    static Settings *instance() { return s_instance; }

public slots:
    void setTimeService(const QString &service);
    void setApiKey(const QString &key);
    void setBreakTimeId(const QString &id);
    void setDescription(const QString &description);
    void setProjectId(const QString &id);
    void setWorkspaceId(const QString &id);
    void setDisableDescription(const bool disable);
    void setUseLastDescription(const bool use);
    void setUseLastProject(const bool use);
    void setShowDurationNotifications(const bool show);
    void setEventLoopInterval(const int interval);

signals:
    void timeServiceChanged();
    void apiKeyChanged();
    void breakTimeIdChanged();
    void descriptionChanged();
    void projectIdChanged();
    void workspaceIdChanged();
    void disableDescriptionChanged();
    void useLastDescriptionChanged();
    void useLastProjectChanged();
    void showDurationNotificationsChanged();
    void eventLoopIntervalChanged();

private:
    explicit Settings(QObject *parent = nullptr);
    ~Settings();

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

    bool m_disableDescription;
    bool m_useLastDescription;
    bool m_useLastProject;
    bool m_showDurationNotifications;

    int m_eventLoopInterval;
};

#endif // SETTINGS_H
