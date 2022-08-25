#include "Settings.h"

#include <QCoreApplication>
#include <QSettings>

#include "Logger.h"

namespace logs = TimeLight::logs;

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

    auto apiKeyJob = new QKeychain::ReadPasswordJob{QCoreApplication::applicationName(), this};
    apiKeyJob->setAutoDelete(true);
    apiKeyJob->setInsecureFallback(true);
    apiKeyJob->setKey(m_timeService + "/apiKey");

    auto l = new QEventLoop{this};
    connect(apiKeyJob, &QKeychain::ReadPasswordJob::finished, this, [this, l, apiKeyJob] {
        if (const auto e = apiKeyJob->error(); e && e != QKeychain::Error::EntryNotFound)
            logs::app()->debug("Could not load API key from secret storage: {}", apiKeyJob->errorString().toStdString());
        else
        {
            m_apiKey = apiKeyJob->textData();
            logs::app()->trace("Loaded API key from secret storage");
        }

        l->quit();
    });
    apiKeyJob->start();
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
    m_middleClickForBreak = settings.value(QStringLiteral("middleClickForBreak"), false).toBool();
    m_eventLoopInterval = settings.value(QStringLiteral("eventLoopInterval"), 5000).toInt();
    m_preventSplittingEntries = settings.value(QStringLiteral("preventSplittingEntries"), true).toBool();
    m_showDurationNotifications = settings.value(QStringLiteral("showDurationNotifications"), true).toBool();
    m_alertOnTimeUp = settings.value(QStringLiteral("alertOnTimeUp"), true).toBool();
    m_weekHours = settings.value(QStringLiteral("weekHours"), 40.).toDouble();
    m_developerMode = settings.value(QStringLiteral("developerMode"), false).toBool();
    settings.endGroup();

    settings.beginGroup(QStringLiteral("teams"));
    m_useTeamsIntegration = settings.value(QStringLiteral("useTeamsIntegration"), false).toBool();
    m_presenceWhileWorking =
        settings.value(QStringLiteral("presenceWhileWorking"), static_cast<unsigned int>(TeamsClient::Presence::Available))
            .value<TeamsClient::Presence>();
    m_presenceWhileOnBreak =
        settings.value(QStringLiteral("presenceWhileOnBreak"), static_cast<unsigned int>(TeamsClient::Presence::Away))
            .value<TeamsClient::Presence>();
    m_presenceWhileNotWorking =
        settings.value(QStringLiteral("presenceWhileNotWorking"), static_cast<unsigned int>(TeamsClient::Presence::Away))
            .value<TeamsClient::Presence>();

    auto accessTokenJob = new QKeychain::ReadPasswordJob{QCoreApplication::applicationName(), this};
    accessTokenJob->setAutoDelete(true);
    accessTokenJob->setInsecureFallback(true);
    accessTokenJob->setKey("microsoft-graph-access-token");
    connect(accessTokenJob, &QKeychain::ReadPasswordJob::finished, this, [this, l, accessTokenJob] {
        if (const auto e = accessTokenJob->error(); e && e != QKeychain::Error::EntryNotFound)
            logs::app()->debug("Could not load Graph access token from secret storage: {}",
                               accessTokenJob->errorString().toStdString());
        else
        {
            m_graphAccessToken = accessTokenJob->textData();
            logs::app()->trace("Loaded Graph access token from secret storage");
        }

        l->quit();
    });
    accessTokenJob->start();
    l->exec();

    auto refreshTokenJob = new QKeychain::ReadPasswordJob{QCoreApplication::applicationName(), this};
    refreshTokenJob->setAutoDelete(true);
    refreshTokenJob->setInsecureFallback(true);
    refreshTokenJob->setKey("microsoft-graph-refresh-token");
    connect(refreshTokenJob, &QKeychain::ReadPasswordJob::finished, this, [this, l, refreshTokenJob] {
        if (const auto e = refreshTokenJob->error(); e && e != QKeychain::Error::EntryNotFound)
            logs::app()->debug("Could not load Graph refresh token from secret storage: {}",
                               refreshTokenJob->errorString().toStdString());
        else
        {
            m_graphRefreshToken = refreshTokenJob->textData();
            logs::app()->trace("Loaded Graph refresh token from secret storage");
        }

        l->quit();
        l->deleteLater();
    });
    refreshTokenJob->start();
    l->exec();
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

void Settings::setMiddleClickForBreak(const bool use)
{
    if (use == m_middleClickForBreak)
        return;
    m_middleClickForBreak = use;
    emit middleClickForBreakChanged();
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

void Settings::setPreventSplittingEntries(const bool prevent)
{
    if (prevent == m_preventSplittingEntries)
        return;
    m_preventSplittingEntries = prevent;
    emit preventSplittingEntriesChanged();
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

void Settings::setUseTeamsIntegration(const bool state)
{
    if (state == m_useTeamsIntegration)
        return;
    m_useTeamsIntegration = state;
    emit useTeamsIntegrationChanged();
    m_settingsDirty = true;
}

void Settings::setGraphAccessToken(const QString &token)
{
    if (token == m_graphAccessToken)
        return;
    m_graphAccessToken = token;
    emit graphAccessTokenChanged();
    m_settingsDirty = true;
}

void Settings::setGraphRefreshToken(const QString &token)
{
    if (token == m_graphRefreshToken)
        return;
    m_graphRefreshToken = token;
    emit graphRefreshTokenChanged();
    m_settingsDirty = true;
}

void Settings::setPresenceWhileWorking(const TeamsClient::Presence &presence)
{
    if (presence == m_presenceWhileWorking)
        return;
    m_presenceWhileWorking = presence;
    emit presenceWhileWorkingChanged();
    m_settingsDirty = true;
}

void Settings::setPresenceWhileOnBreak(const TeamsClient::Presence &presence)
{
    if (presence == m_presenceWhileOnBreak)
        return;
    m_presenceWhileOnBreak = presence;
    emit presenceWhileOnBreakChanged();
    m_settingsDirty = true;
}

void Settings::setPresenceWhileNotWorking(const TeamsClient::Presence &presence)
{
    if (presence == m_presenceWhileNotWorking)
        return;
    m_presenceWhileNotWorking = presence;
    emit presenceWhileNotWorkingChanged();
    m_settingsDirty = true;
}

void Settings::save(bool async)
{
    auto apiKeyJob = new QKeychain::WritePasswordJob{QCoreApplication::applicationName(), this};
    apiKeyJob->setAutoDelete(true);
    apiKeyJob->setInsecureFallback(true);
    apiKeyJob->setKey(m_timeService + "/apiKey");
    apiKeyJob->setTextData(m_apiKey);

    auto l = new QEventLoop{this};
    connect(apiKeyJob, &QKeychain::WritePasswordJob::finished, apiKeyJob, [l](QKeychain::Job *job) {
        if (job->error())
            logs::app()->error("Failed to save API key to secret storage: {}", job->errorString().toStdString());
        else
            logs::app()->trace("Saved API key to secret storage");
        if (l)
            l->quit();
    });
    apiKeyJob->start();
    if (!async)
        l->exec();

    QSettings settings;

    settings.setValue(QStringLiteral("timeService"), m_timeService);

    settings.beginGroup(m_timeService);

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
    settings.setValue(QStringLiteral("middleClickForBreak"), m_middleClickForBreak);
    settings.setValue(QStringLiteral("eventLoopInterval"), m_eventLoopInterval);
    settings.setValue(QStringLiteral("preventSplittingEntries"), m_preventSplittingEntries);
    settings.setValue(QStringLiteral("showDurationNotifications"), m_showDurationNotifications);
    settings.setValue(QStringLiteral("alertOnTimeUp"), m_alertOnTimeUp);
    settings.setValue(QStringLiteral("weekHours"), m_weekHours);
    settings.setValue(QStringLiteral("developerMode"), m_developerMode);
    settings.endGroup();

    settings.beginGroup(QStringLiteral("teams"));
    settings.setValue(QStringLiteral("useTeamsIntegration"), m_useTeamsIntegration);
    settings.setValue(QStringLiteral("presenceWhileWorking"), static_cast<unsigned int>(m_presenceWhileWorking));
    settings.setValue(QStringLiteral("presenceWhileOnBreak"), static_cast<unsigned int>(m_presenceWhileOnBreak));
    settings.setValue(QStringLiteral("presenceWhileNotWorking"), static_cast<unsigned int>(m_presenceWhileNotWorking));

    auto accessTokenJob = new QKeychain::WritePasswordJob{QCoreApplication::applicationName(), this};
    accessTokenJob->setAutoDelete(true);
    accessTokenJob->setInsecureFallback(true);
    accessTokenJob->setKey("microsoft-graph-access-token");
    accessTokenJob->setTextData(m_graphAccessToken);
    connect(accessTokenJob, &QKeychain::WritePasswordJob::finished, accessTokenJob, [l](QKeychain::Job *job) {
        if (job->error())
            logs::app()->error("Failed to save Graph access token to secret storage: {}", job->errorString().toStdString());
        else
            logs::app()->trace("Saved Graph access token to secret storage");
        if (l)
            l->quit();
    });
    accessTokenJob->start();
    if (!async)
        l->exec();

    auto refreshTokenJob = new QKeychain::WritePasswordJob{QCoreApplication::applicationName(), this};
    refreshTokenJob->setAutoDelete(true);
    refreshTokenJob->setInsecureFallback(true);
    refreshTokenJob->setKey("microsoft-graph-refresh-token");
    refreshTokenJob->setTextData(m_graphRefreshToken);
    connect(refreshTokenJob, &QKeychain::WritePasswordJob::finished, refreshTokenJob, [l, this](QKeychain::Job *job) {
        if (job->error())
            logs::app()->error("Failed to save Graph refresh token to secret storage: {}", job->errorString().toStdString());
        else
            logs::app()->trace("Saved Graph refresh token to secret storage");
        if (l)
        {
            l->quit();
            l->deleteLater();
        }
    });
    refreshTokenJob->start();
    if (!async)
        l->exec();
    settings.endGroup();
}
