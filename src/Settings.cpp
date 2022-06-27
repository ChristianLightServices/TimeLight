#include "Settings.h"

#include <QCoreApplication>
#include <QSettings>

#include <iostream>

#if __has_include(<keychain.h>)
    #include <keychain.h>
#else
    #include <qt6keychain/keychain.h>
#endif

Settings::Settings(QObject *parent)
    : QObject{parent}
{
    load();

    m_saveTimer.setInterval(10000);
    m_saveTimer.setSingleShot(false);
    m_saveTimer.callOnTimeout(this, [this] {
        if (m_settingsDirty)
        {
            save();
            m_settingsDirty = false;
        }
    });
    m_saveTimer.start();

    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { save(false); });
}

Settings::~Settings()
{
    save(false);
}

void Settings::load()
{
    QSettings settings;

    m_timeService = settings.value(QStringLiteral("timeService")).toString();

    auto job = new QKeychain::ReadPasswordJob{QCoreApplication::applicationName(), this};
    job->setAutoDelete(true);
    job->setInsecureFallback(true);
    job->setKey(m_timeService + "/apiKey");

    auto l = new QEventLoop{this};
    connect(job, &QKeychain::ReadPasswordJob::finished, this, [this, l, job](QKeychain::Job *) {
        if (job->error())
        {
            std::cout << "Could not load API key from secret storage: " << job->errorString().toStdString() << std::endl;

            // TODO: delete this migration after a while
            QSettings settings;
            if (settings.contains(job->key()))
                m_apiKey = settings.value(job->key()).toString();
        }
        else
            m_apiKey = job->textData();

        l->quit();
        l->deleteLater();
    });
    job->start();
    l->exec();

    settings.beginGroup(m_timeService);
    m_breakTimeId = settings.value(QStringLiteral("breakTimeId")).toString();
    m_description = settings.value(QStringLiteral("description")).toString();
    m_disableDescription = settings.value(QStringLiteral("disableDescription"), false).toBool();
    m_projectId = settings.value(QStringLiteral("projectId")).toString();
    m_useLastDescription = settings.value(QStringLiteral("useLastDescription"), true).toBool();
    m_useLastProject = settings.value(QStringLiteral("useLastProject"), true).toBool();
    m_useSeparateBreakTime = settings.value(QStringLiteral("useSeparateBreakTime"), false).toBool();
    m_workspaceId = settings.value(QStringLiteral("workspaceId")).toString();
    settings.endGroup();

    settings.beginGroup(QStringLiteral("app"));
    m_eventLoopInterval = settings.value(QStringLiteral("eventLoopInterval"), 3000).toInt();
    m_showDurationNotifications = settings.value(QStringLiteral("showDurationNotifications"), true).toBool();
    m_quickStartProjectsLoading =
        static_cast<QuickStartProjectOptions>(settings
                                                  .value(QStringLiteral("quickStartProjectsLoading"),
                                                         static_cast<unsigned int>(QuickStartProjectOptions::AllProjects))
                                                  .toUInt());
    if (m_quickStartProjectsLoading >= QuickStartProjectOptions::Undefined ||
        m_quickStartProjectsLoading < static_cast<QuickStartProjectOptions>(0))
        m_quickStartProjectsLoading = QuickStartProjectOptions::AllProjects;
    m_alertOnTimeUp = settings.value(QStringLiteral("alertOnTimeUp"), true).toBool();
    m_weekHours = settings.value(QStringLiteral("weekHours"), 40.).toDouble();
    m_developerMode = settings.value(QStringLiteral("developerMode"), false).toBool();
    settings.endGroup();
}

void Settings::init()
{
    if (s_instance)
        delete s_instance;
    s_instance = new Settings;
}

void Settings::setTimeService(const QString &service)
{
    if (service == m_timeService)
        return;

    // this hack makes sure that time-service-specific settings don't contaminate other services' settings
    save();
    QSettings settings;
    settings.setValue(QStringLiteral("timeService"), service);
    load();

    emit timeServiceChanged();
}

void Settings::setApiKey(const QString &key)
{
    if (key == m_apiKey)
        return;
    m_apiKey = key;
    emit apiKeyChanged();
    m_settingsDirty = true;
}

void Settings::setBreakTimeId(const QString &id)
{
    if (id == m_breakTimeId)
        return;
    m_breakTimeId = id;
    emit breakTimeIdChanged();
    m_settingsDirty = true;
}

void Settings::setDescription(const QString &description)
{
    if (description == m_description)
        return;
    m_description = description;
    emit descriptionChanged();
    m_settingsDirty = true;
}

void Settings::setProjectId(const QString &id)
{
    if (id == m_projectId)
        return;
    m_projectId = id;
    emit projectIdChanged();
    m_settingsDirty = true;
}

void Settings::setWorkspaceId(const QString &id)
{
    if (id == m_workspaceId)
        return;
    m_workspaceId = id;
    emit workspaceIdChanged();
    m_settingsDirty = true;
}

void Settings::setUseSeparateBreakTime(const bool use)
{
    if (use == m_useSeparateBreakTime)
        return;
    m_useSeparateBreakTime = use;
    emit useSeparateBreakTimeChanged();
    m_settingsDirty = true;
}

void Settings::setDisableDescription(const bool disable)
{
    if (disable == m_disableDescription)
        return;
    m_disableDescription = disable;
    emit disableDescriptionChanged();
    m_settingsDirty = true;
}

void Settings::setUseLastDescription(const bool use)
{
    if (use == m_useLastDescription)
        return;
    m_useLastDescription = use;
    emit useLastDescriptionChanged();
    m_settingsDirty = true;
}

void Settings::setUseLastProject(const bool use)
{
    if (use == m_useLastProject)
        return;
    m_useLastProject = use;
    emit useLastProjectChanged();
    m_settingsDirty = true;
}

void Settings::setShowDurationNotifications(const bool show)
{
    if (show == m_showDurationNotifications)
        return;
    m_showDurationNotifications = show;
    emit showDurationNotificationsChanged();
    m_settingsDirty = true;
}

void Settings::setEventLoopInterval(const int interval)
{
    if (interval == m_eventLoopInterval)
        return;
    m_eventLoopInterval = interval;
    emit eventLoopIntervalChanged();
    m_settingsDirty = true;
}

void Settings::setQuickStartProjectsLoading(const QuickStartProjectOptions &option)
{
    if (option == m_quickStartProjectsLoading)
        return;
    m_quickStartProjectsLoading = option;
    emit quickStartProjectsLoadingChanged();
    m_settingsDirty = true;
}

void Settings::setAlertOnTimeUp(const bool alert)
{
    if (alert == m_alertOnTimeUp)
        return;
    m_alertOnTimeUp = alert;
    emit alertOnTimeUpChanged();
    m_settingsDirty = true;
}

void Settings::setWeekHours(const double hours)
{
    if (hours == m_weekHours)
        return;
    m_weekHours = hours;
    emit weekHoursChanged();
    m_settingsDirty = true;
}

void Settings::setDeveloperMode(const bool state)
{
    if (state == m_developerMode)
        return;
    m_developerMode = state;
    emit developerModeChanged();
    m_settingsDirty = true;
}

void Settings::save(bool async)
{
    auto job = new QKeychain::WritePasswordJob{QCoreApplication::applicationName(), this};
    job->setAutoDelete(true);
    job->setInsecureFallback(true);
    job->setKey(m_timeService + "/apiKey");
    job->setTextData(m_apiKey);

    auto l = new QEventLoop{this};
    connect(job, &QKeychain::WritePasswordJob::finished, job, [l](QKeychain::Job *job) {
        if (job->error())
            std::cerr << "Failed to save API key to secret storage: " << job->errorString().toStdString() << std::endl;
        l->quit();
        l->deleteLater();
    });
    job->start();
    if (!async)
        l->exec();

    QSettings settings;

    settings.setValue(QStringLiteral("timeService"), m_timeService);

    settings.beginGroup(m_timeService);

    // Since the API key is now handled by qtkeychain, we'll delete the old copy.
    settings.remove(QStringLiteral("apiKey"));
    // TODO: Delete the above migration after an appropriate amount of time.

    settings.setValue(QStringLiteral("breakTimeId"), m_breakTimeId);
    settings.setValue(QStringLiteral("description"), m_description);
    settings.setValue(QStringLiteral("disableDescription"), m_disableDescription);
    settings.setValue(QStringLiteral("projectId"), m_projectId);
    settings.setValue(QStringLiteral("useLastDescription"), m_useLastDescription);
    settings.setValue(QStringLiteral("useLastProject"), m_useLastProject);
    settings.setValue(QStringLiteral("useSeparateBreakTime"), m_useSeparateBreakTime);
    settings.setValue(QStringLiteral("workspaceId"), m_workspaceId);
    settings.endGroup();

    settings.beginGroup(QStringLiteral("app"));
    settings.setValue(QStringLiteral("eventLoopInterval"), m_eventLoopInterval);
    settings.setValue(QStringLiteral("showDurationNotifications"), m_showDurationNotifications);
    settings.setValue(QStringLiteral("quickStartProjectsLoading"), static_cast<unsigned int>(m_quickStartProjectsLoading));
    settings.setValue(QStringLiteral("alertOnTimeUp"), m_alertOnTimeUp);
    settings.setValue(QStringLiteral("weekHours"), m_weekHours);
    settings.setValue(QStringLiteral("developerMode"), m_developerMode);
    settings.endGroup();
}
