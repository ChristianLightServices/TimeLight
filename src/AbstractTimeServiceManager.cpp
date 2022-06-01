#include "AbstractTimeServiceManager.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <iostream>

#include "JsonHelper.h"
#include "Project.h"
#include "TimeEntry.h"
#include "User.h"
#include "Workspace.h"

const NetworkReplyCallback AbstractTimeServiceManager::s_defaultSuccessCb = [](QNetworkReply *) {};
const NetworkReplyCallback AbstractTimeServiceManager::s_defaultFailureCb = [](QNetworkReply *reply) {
    if (auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); code == 0)
        std::cerr << "Internet connection lost" << std::endl;
    else
        std::cerr << "Request " << reply->url().toString().toStdString() << " failed with code " << code << std::endl;
};

class NoCookies : public QNetworkCookieJar
{
    virtual QList<QNetworkCookie> cookiesForUrl(const QUrl &) const override { return {}; }
    virtual bool setCookiesFromUrl(const QList<QNetworkCookie> &, const QUrl &) override { return false; }
};

AbstractTimeServiceManager::AbstractTimeServiceManager(const QByteArray &apiKey, QObject *parent)
    : QObject{parent},
      m_apiKey{apiKey}
{
    m_manager.setCookieJar(new NoCookies);
    m_manager.setAutoDeleteReplies(true);

    m_expireUsersTimer.setInterval(15 * 60 * 1000); // every 15 mins
    m_expireUsersTimer.callOnTimeout(this, [this]() {
        m_usersLoaded = false;
        m_users.clear();
    });
    m_expireUsersTimer.setSingleShot(true);

    m_expireProjectsTimer.setInterval(15 * 60 * 1000);
    m_expireProjectsTimer.callOnTimeout(this, [this]() {
        m_projectsLoaded = false;
        m_projects.clear();
    });
    m_expireProjectsTimer.setSingleShot(true);

    // We won't actually populate the user or project list yet; why do it when we probably won't need it for a long time?
}

QVector<Project> &AbstractTimeServiceManager::projects()
{
    if (!m_projectsLoaded)
        updateProjects();

    return m_projects;
}

QVector<QPair<QString, QString>> &AbstractTimeServiceManager::users()
{
    if (!m_usersLoaded)
        updateUsers();

    return m_users;
}

QString AbstractTimeServiceManager::projectName(const QString &projectId)
{
    for (auto &item : projects())
        if (item.id() == projectId)
            return item.name();

    return "";
}

QString AbstractTimeServiceManager::userName(const QString &userId)
{
    for (const auto &item : qAsConst(users()))
        if (item.first == userId)
            return item.second;

    return "";
}

QDateTime AbstractTimeServiceManager::stopRunningTimeEntry(const QString &userId, bool async)
{
    auto now = currentDateTime();
    auto t = getRunningTimeEntry(userId);
    if (t)
    {
        t->setEnd(now);
        timeEntryReq(stopTimeEntryUrl(userId, m_workspaceId),
                     TimeEntryAction::StopTimeEntry,
                     QByteArray::fromStdString(timeEntryToJson(*t, TimeEntryAction::StopTimeEntry).dump()),
                     async);
    }
    return now;
}

std::optional<TimeEntry> AbstractTimeServiceManager::getRunningTimeEntry(const QString &userId)
{
    json j;
    timeEntryReq(runningTimeEntryUrl(userId, m_workspaceId),
                 TimeEntryAction::GetRunningTimeEntry,
                 getRunningTimeEntryPayload(),
                 false,
                 [&j](QNetworkReply *rep) {
                     try
                     {
                         j = json::parse(rep->readAll().toStdString());
                     }
                     catch (...)
                     {
                         j = {};
                     }
                 });

    if (j.is_null())
        return std::nullopt;
    else
        return jsonToRunningTimeEntry(j.is_array() ? j[0] : j);
}

void AbstractTimeServiceManager::startTimeEntry(const QString &userId, const QString &projectId, bool async)
{
    startTimeEntry(userId, projectId, QString{}, currentDateTime(), async);
}

void AbstractTimeServiceManager::startTimeEntry(const QString &userId,
                                                const QString &projectId,
                                                const QString &description,
                                                bool async)
{
    startTimeEntry(userId, projectId, description, currentDateTime(), async);
}

void AbstractTimeServiceManager::startTimeEntry(const QString &userId,
                                                const QString &projectId,
                                                const QDateTime &start,
                                                bool async)
{
    startTimeEntry(userId, projectId, QString{}, start, async);
}

void AbstractTimeServiceManager::startTimeEntry(
    const QString &userId, const QString &projectId, const QString &description, const QDateTime &start, bool async)
{
    Project p;
    auto it =
        std::find_if(m_projects.begin(), m_projects.end(), [&projectId](const Project &p) { return p.id() == projectId; });
    if (it != m_projects.end())
        p = *it;
    else
        p = Project{projectId, projectName(projectId)};
    TimeEntry t{{}, p, description, userId, start, {}};
    timeEntryReq(startTimeEntryUrl(userId, m_workspaceId),
                 TimeEntryAction::StartTimeEntry,
                 QByteArray::fromStdString(timeEntryToJson(t, TimeEntryAction::StartTimeEntry).dump()),
                 async);
}

QVector<TimeEntry> AbstractTimeServiceManager::getTimeEntries(const QString &userId,
                                                              [[maybe_unused]] std::optional<int> pageNumber,
                                                              [[maybe_unused]] std::optional<int> pageSize)
{
    QVector<TimeEntry> entries;

    auto url = timeEntriesUrl(userId, workspaceId());
    auto worker = [this, &url, &entries]() -> bool {
        bool retVal;
        get(url, false, [this, &retVal, &entries](QNetworkReply *rep) {
            try
            {
                json j{json::parse(rep->readAll().toStdString())};

                if (j.is_array() && j[0].is_array())
                    j = j[0];
                if (!j.is_null() && !j[0].is_null())
                    for (const auto &entry : j)
                        entries.push_back(jsonToTimeEntry(entry));

                if (timeEntriesSortOrder() == Qt::AscendingOrder)
                    std::reverse(entries.begin(), entries.end());
                retVal = !j.empty();
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error in parsing time entries: " << e.what() << std::endl;
                retVal = false;
            }
        });
        return retVal;
    };

    if (supportedPagination().testFlag(Pagination::TimeEntries))
    {
        int page = pageNumber.value_or(paginationStartsAt());
        // The loop will only run once if there a specific page number has been requested.
        do
        {
            QUrlQuery query;
            query.addQueryItem(timeEntriesPageSizeHeaderName(),
                               QString::number(pageSize.value_or(timeEntriesPaginationPageSize())));
            query.addQueryItem(timeEntriesPageHeaderName(), QString::number(page++));
            url.setQuery(query);
        } while (worker() && !pageNumber.has_value());
    }
    else
        worker();

    return entries;
}

User AbstractTimeServiceManager::getApiKeyOwner()
{
    if (!m_ownerId.isEmpty()) [[likely]]
        return User{m_ownerId, {}, m_workspaceId, this};
    else
    {
        json j;
        get(currentUserUrl(), false, [&j](QNetworkReply *rep) {
            try
            {
                j = json::parse(rep->readAll().toStdString());
            }
            catch (const std::exception &ex)
            {
                std::cout << ex.what() << std::endl;
            }
        });

        return j.empty() ? User{this} : jsonToUser(j);
    }
}

QVector<Workspace> AbstractTimeServiceManager::getOwnerWorkspaces()
{
    QVector<Workspace> workspaces;

    get(workspacesUrl(), false, [this, &workspaces](QNetworkReply *rep) {
        try
        {
            json j{json::parse(rep->readAll().toStdString())};
            using namespace std::placeholders;
            //			std::transform(j.begin(), j.end(), workspaces.begin(),
            // std::bind(&AbstractTimeServiceManager::jsonToWorkspace, this, _1));
            for (const auto &workspace : j)
                workspaces.push_back(jsonToWorkspace(workspace));
        }
        catch (const std::exception &ex)
        {
            std::cerr << "Error while loading workspaces: " << ex.what() << std::endl;
        }
    });

    return workspaces;
}

void AbstractTimeServiceManager::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey.toUtf8();
    updateCurrentUser();
    emit apiKeyChanged();
}

void AbstractTimeServiceManager::setWorkspaceId(const QString &workspaceId)
{
    // TODO: validate this
    //	auto it = std::find_if(m_workspaces.begin(), m_workspaces.end(), [&workspaceId](Workspace &w) { return w.id() ==
    // workspaceId; }); 	if (it != m_workspaces.end)
    m_workspaceId = workspaceId;
    m_projectsLoaded = false;
    m_usersLoaded = false;
}

void AbstractTimeServiceManager::callInitVirtualMethods()
{
    // request currently logged in user (the one whose API key we're using) as a validity test
    // and also in order to cache API key info
    updateCurrentUser();

    auto checkInternet = [this] {
        // TODO: possibly abstract this URL?
        head(
            QUrl{"https://clockify.me"},
            [this](QNetworkReply *) {
                // a changed internet connection is rather unlikely
                if (!m_isConnectedToInternet) [[unlikely]]
                {
                    m_isConnectedToInternet = true;
                    emit internetConnectionChanged(m_isConnectedToInternet);
                }
            },
            [this](QNetworkReply *) {
                if (m_isConnectedToInternet) [[unlikely]]
                {
                    m_isConnectedToInternet = false;
                    emit internetConnectionChanged(m_isConnectedToInternet);
                }
            });
    };
    checkInternet();
    m_checkConnectionTimer.setInterval(5000); // check connection every 5s, since it's not that likely to change a lot
    m_checkConnectionTimer.callOnTimeout(checkInternet);
    m_checkConnectionTimer.setSingleShot(false);
    m_checkConnectionTimer.start();
}

void AbstractTimeServiceManager::updateCurrentUser()
{
    get(
        currentUserUrl(),
        false,
        [this](QNetworkReply *rep) {
            try
            {
                json j{json::parse(rep->readAll().toStdString())};
                auto user = jsonToUser(j.is_array() ? j[0] : j);
                m_isValid = user.isValid();
            }
            catch (const std::exception &ex)
            {
                std::cerr << "Error: " << ex.what() << std::endl;
            }
        },
        [this](QNetworkReply *rep) {
            if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == 0) [[likely]]
                m_isValid = true; // hopefully the connection will work once the internet is connected
            s_defaultFailureCb(rep);
        });
}

void AbstractTimeServiceManager::updateUsers()
{
    if (m_loadingUsers)
        return;

    m_loadingUsers = true;
    m_usersLoaded = false;
    m_users.clear();

    auto url = usersUrl(workspaceId());
    auto worker = [this, &url]() -> bool {
        bool retCode;
        get(url, [this, &retCode](QNetworkReply *rep) {
            try
            {
                json j{json::parse(rep->readAll().toStdString())};

                using namespace std::placeholders;
                //				std::transform(j.begin(), j.end(), m_users.begin(),
                // std::bind(&AbstractTimeServiceManager::jsonToUserData, this, _1));
                for (const auto &user : j)
                    m_users.push_back(jsonToUserData(user));

                retCode = !j.empty();
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error in parsing users: " << e.what() << std::endl;
                retCode = false;
            }
        });
        return retCode;
    };

    if (supportedPagination().testFlag(Pagination::Users))
    {
        int page = paginationStartsAt();

        do
        {
            QUrlQuery query;
            query.addQueryItem(usersPageSizeHeaderName(), QString::number(usersPaginationPageSize()));
            query.addQueryItem(usersPageHeaderName(), QString::number(page++));
            url.setQuery(query);
        } while (worker());
    }
    else // who cares about the return code?
        worker();

    m_usersLoaded = true;
    m_loadingUsers = false;
    m_expireUsersTimer.start();
}

void AbstractTimeServiceManager::updateProjects()
{
    if (m_loadingProjects)
        return;

    m_loadingProjects = true;
    m_projectsLoaded = false;
    m_projects.clear();

    auto url = projectsUrl(m_workspaceId);
    auto worker = [this, &url]() -> bool {
        bool retVal;
        get(url, false, [this, &retVal](QNetworkReply *rep) {
            try
            {
                json j{json::parse(rep->readAll().toStdString())};
                if (j.empty() || j.is_null())
                {
                    retVal = false;
                    return;
                }

                //				using namespace std::placeholders;
                //				std::transform(j.begin(), j.end(), m_projects.begin(),
                // std::bind(&AbstractTimeServiceManager::jsonToProject, this, _1));
                // I'm not sure why, but nlohmann::json tends to make everything into an array
                if (j.is_array() && j[0].is_array())
                    j = j[0];
                for (const auto &project : j)
                    m_projects.push_back(jsonToProject(project));

                retVal = !j.empty() && !j.is_null();
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error in parsing projects: " << e.what() << std::endl;
                retVal = false;
            }
        });
        return retVal;
    };

    if (supportedPagination().testFlag(Pagination::Projects))
    {
        int page = paginationStartsAt();

        do
        {
            QUrlQuery query;
            query.addQueryItem(projectsPageSizeHeaderName(), QString::number(projectsPaginationPageSize()));
            query.addQueryItem(projectsPageHeaderName(), QString::number(page++));
            url.setQuery(query);
        } while (worker());
    }
    else
        worker();

    m_projectsLoaded = true;
    m_loadingProjects = false;
    m_expireProjectsTimer.start();
}

void AbstractTimeServiceManager::timeEntryReq(const QUrl &url,
                                              const TimeEntryAction action,
                                              const QByteArray &body,
                                              bool async,
                                              const NetworkReplyCallback &successCb,
                                              const NetworkReplyCallback &failureCb)
{
    auto verb = httpVerbForAction(action);
    if (verb == HttpVerb::Get)
        get(url, async, httpReturnCodeForVerb(verb), successCb, failureCb);
    else if (verb == HttpVerb::Post)
        post(url, body, async, httpReturnCodeForVerb(verb), successCb, failureCb);
    else if (verb == HttpVerb::Patch)
        patch(url, body, async, httpReturnCodeForVerb(verb), successCb, failureCb);
    else if (verb == HttpVerb::Put)
        put(url, body, async, httpReturnCodeForVerb(verb), successCb, failureCb);
    else if (verb == HttpVerb::Head)
        head(url, async, httpReturnCodeForVerb(verb), successCb, failureCb);
}

QDateTime AbstractTimeServiceManager::jsonToDateTime(const nlohmann::json &j) const
{
    try
    {
        return QDateTime::fromString(j.get<QString>(), jsonTimeFormatString());
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error in parsing time: " << e.what() << std::endl;
        return {};
    }
}

json AbstractTimeServiceManager::dateTimeToJson(const QDateTime &dt) const
{
    return dt.toString(jsonTimeFormatString());
}

// *****************************************************************************
// *****     BEYOND THIS POINT THERE ARE ONLY FUNCTIONS FOR REST STUFF     *****
// *****************************************************************************

void AbstractTimeServiceManager::get(const QUrl &url,
                                     const NetworkReplyCallback &successCb,
                                     const NetworkReplyCallback &failureCb)
{
    get(url, true, 200, successCb, failureCb);
}

void AbstractTimeServiceManager::get(const QUrl &url,
                                     int expectedReturnCode,
                                     const NetworkReplyCallback &successCb,
                                     const NetworkReplyCallback &failureCb)
{
    get(url, true, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::get(const QUrl &url,
                                     bool async,
                                     const NetworkReplyCallback &successCb,
                                     const NetworkReplyCallback &failureCb)
{
    get(url, async, 200, successCb, failureCb);
}

void AbstractTimeServiceManager::get(const QUrl &url,
                                     bool async,
                                     int expectedReturnCode,
                                     const NetworkReplyCallback &successCb,
                                     const NetworkReplyCallback &failureCb)
{
    QNetworkRequest req{url};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader(authHeaderName(), apiKeyForRequests());

    auto rep = m_manager.get(req);
    m_pendingReplies.insert(rep, {successCb, failureCb});

    bool *done = async ? nullptr : new bool{false};

    connect(
        rep,
        &QNetworkReply::finished,
        this,
        [this, rep, expectedReturnCode, done]() {
            if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == expectedReturnCode)
                [[likely]]
            {
                if (m_isRatelimited)
                {
                    m_isRatelimited = false;
                    emit ratelimited(false);
                }
                m_pendingReplies[rep].first(rep);
            }
            else [[unlikely]]
            {
                if (status == 0) [[likely]]
                {
                    m_isConnectedToInternet = false;
                    emit internetConnectionChanged(false);
                }
                else if (status == 429)
                {
                    m_isRatelimited = true;
                    emit ratelimited(true);
                }
                m_pendingReplies[rep].second(rep);
            }

            if (done)
                *done = true;

            m_pendingReplies.remove(rep);
        },
        Qt::DirectConnection);

    if (!async)
        while (!(*done))
        {
            qApp->processEvents();
            qApp->sendPostedEvents();
        }

    if (done)
        delete done;
}

void AbstractTimeServiceManager::post(const QUrl &url,
                                      const QByteArray &body,
                                      const NetworkReplyCallback &successCb,
                                      const NetworkReplyCallback &failureCb)
{
    post(url, body, true, 200, successCb, failureCb);
}

void AbstractTimeServiceManager::post(const QUrl &url,
                                      const QByteArray &body,
                                      int expectedReturnCode,
                                      const NetworkReplyCallback &successCb,
                                      const NetworkReplyCallback &failureCb)
{
    post(url, body, true, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::post(const QUrl &url,
                                      const QByteArray &body,
                                      bool async,
                                      const NetworkReplyCallback &successCb,
                                      const NetworkReplyCallback &failureCb)
{
    post(url, body, async, 200, successCb, failureCb);
}

void AbstractTimeServiceManager::post(const QUrl &url,
                                      const QByteArray &body,
                                      bool async,
                                      int expectedReturnCode,
                                      const NetworkReplyCallback &successCb,
                                      const NetworkReplyCallback &failureCb)
{
    QNetworkRequest req{url};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader(authHeaderName(), apiKeyForRequests());

    auto rep = m_manager.post(req, body);
    m_pendingReplies.insert(rep, {successCb, failureCb});

    bool *done = async ? nullptr : new bool{false};

    connect(
        rep,
        &QNetworkReply::finished,
        this,
        [this, rep, expectedReturnCode, done]() {
            if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == expectedReturnCode)
                [[likely]]
            {
                if (m_isRatelimited)
                {
                    m_isRatelimited = false;
                    emit ratelimited(false);
                }
                m_pendingReplies[rep].first(rep);
            }
            else [[unlikely]]
            {
                if (status == 0) [[likely]]
                {
                    m_isConnectedToInternet = false;
                    emit internetConnectionChanged(false);
                }
                else if (status == 429)
                {
                    m_isRatelimited = true;
                    emit ratelimited(true);
                }
                m_pendingReplies[rep].second(rep);
            }

            if (done)
                *done = true;

            m_pendingReplies.remove(rep);
        },
        Qt::DirectConnection);

    if (!async)
        while (!(*done))
        {
            qApp->processEvents();
            qApp->sendPostedEvents();
        }

    if (done)
        delete done;
}

void AbstractTimeServiceManager::patch(const QUrl &url,
                                       const QByteArray &body,
                                       const NetworkReplyCallback &successCb,
                                       const NetworkReplyCallback &failureCb)
{
    patch(url, body, true, 200, successCb, failureCb);
}

void AbstractTimeServiceManager::patch(const QUrl &url,
                                       const QByteArray &body,
                                       int expectedReturnCode,
                                       const NetworkReplyCallback &successCb,
                                       const NetworkReplyCallback &failureCb)
{
    patch(url, body, true, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::patch(const QUrl &url,
                                       const QByteArray &body,
                                       bool async,
                                       const NetworkReplyCallback &successCb,
                                       const NetworkReplyCallback &failureCb)
{
    patch(url, body, async, 200, successCb, failureCb);
}

void AbstractTimeServiceManager::patch(const QUrl &url,
                                       const QByteArray &body,
                                       bool async,
                                       int expectedReturnCode,
                                       const NetworkReplyCallback &successCb,
                                       const NetworkReplyCallback &failureCb)
{
    QNetworkRequest req{url};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader(authHeaderName(), m_apiKey);
    req.setRawHeader("Accept", "application/json");

    auto rep = m_manager.sendCustomRequest(req, QByteArrayLiteral("PATCH"), body);
    m_pendingReplies.insert(rep, {successCb, failureCb});

    bool *done = async ? nullptr : new bool{false};

    connect(
        rep,
        &QNetworkReply::finished,
        this,
        [this, rep, expectedReturnCode, done]() {
            if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == expectedReturnCode)
                [[likely]]
            {
                if (m_isRatelimited)
                {
                    m_isRatelimited = false;
                    emit ratelimited(false);
                }
                m_pendingReplies[rep].first(rep);
            }
            else [[unlikely]]
            {
                if (status == 0) [[likely]]
                {
                    m_isConnectedToInternet = false;
                    emit internetConnectionChanged(false);
                }
                else if (status == 429)
                {
                    m_isRatelimited = true;
                    emit ratelimited(true);
                }
                m_pendingReplies[rep].second(rep);
            }

            if (done)
                *done = true;

            m_pendingReplies.remove(rep);
        },
        Qt::DirectConnection);

    if (!async)
        while (!(*done))
        {
            qApp->processEvents();
            qApp->sendPostedEvents();
        }

    if (done)
        delete done;
}

void AbstractTimeServiceManager::put(const QUrl &url,
                                     const QByteArray &body,
                                     const NetworkReplyCallback &successCb,
                                     const NetworkReplyCallback &failureCb)
{
    put(url, body, true, 200, successCb, failureCb);
}

void AbstractTimeServiceManager::put(const QUrl &url,
                                     const QByteArray &body,
                                     int expectedReturnCode,
                                     const NetworkReplyCallback &successCb,
                                     const NetworkReplyCallback &failureCb)
{
    put(url, body, true, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::put(const QUrl &url,
                                     const QByteArray &body,
                                     bool async,
                                     const NetworkReplyCallback &successCb,
                                     const NetworkReplyCallback &failureCb)
{
    put(url, body, async, 200, successCb, failureCb);
}

void AbstractTimeServiceManager::put(const QUrl &url,
                                     const QByteArray &body,
                                     bool async,
                                     int expectedReturnCode,
                                     const NetworkReplyCallback &successCb,
                                     const NetworkReplyCallback &failureCb)
{
    QNetworkRequest req{url};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader(authHeaderName(), apiKeyForRequests());

    auto rep = m_manager.put(req, body);
    m_pendingReplies.insert(rep, {successCb, failureCb});

    bool *done = async ? nullptr : new bool{false};

    connect(
        rep,
        &QNetworkReply::finished,
        this,
        [this, rep, expectedReturnCode, done]() {
            if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == expectedReturnCode)
                [[likely]]
            {
                if (m_isRatelimited)
                {
                    m_isRatelimited = false;
                    emit ratelimited(false);
                }
                m_pendingReplies[rep].first(rep);
            }
            else [[unlikely]]
            {
                if (status == 0) [[likely]]
                {
                    m_isConnectedToInternet = false;
                    emit internetConnectionChanged(false);
                }
                else if (status == 429)
                {
                    m_isRatelimited = true;
                    emit ratelimited(true);
                }
                m_pendingReplies[rep].second(rep);
            }

            if (done)
                *done = true;

            m_pendingReplies.remove(rep);
        },
        Qt::DirectConnection);

    if (!async)
        while (!(*done))
        {
            qApp->processEvents();
            qApp->sendPostedEvents();
        }

    if (done)
        delete done;
}

void AbstractTimeServiceManager::head(const QUrl &url,
                                      const NetworkReplyCallback &successCb,
                                      const NetworkReplyCallback &failureCb)
{
    head(url, true, 200, successCb, failureCb);
}

void AbstractTimeServiceManager::head(const QUrl &url,
                                      int expectedReturnCode,
                                      const NetworkReplyCallback &successCb,
                                      const NetworkReplyCallback &failureCb)
{
    head(url, true, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::head(const QUrl &url,
                                      bool async,
                                      const NetworkReplyCallback &successCb,
                                      const NetworkReplyCallback &failureCb)
{
    head(url, async, 200, successCb, failureCb);
}

void AbstractTimeServiceManager::head(const QUrl &url,
                                      bool async,
                                      int expectedReturnCode,
                                      const NetworkReplyCallback &successCb,
                                      const NetworkReplyCallback &failureCb)
{
    QNetworkRequest req{url};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader(authHeaderName(), apiKeyForRequests());

    auto rep = m_manager.head(req);
    m_pendingReplies.insert(rep, {successCb, failureCb});

    bool *done = async ? nullptr : new bool{false};

    connect(
        rep,
        &QNetworkReply::finished,
        this,
        [this, rep, expectedReturnCode, done]() {
            if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == expectedReturnCode)
                [[likely]]
            {
                if (m_isRatelimited)
                {
                    m_isRatelimited = false;
                    emit ratelimited(false);
                }
                m_pendingReplies[rep].first(rep);
            }
            else [[unlikely]]
            {
                if (status == 0) [[likely]]
                {
                    m_isConnectedToInternet = false;
                    emit internetConnectionChanged(false);
                }
                else if (status == 429)
                {
                    m_isRatelimited = true;
                    emit ratelimited(true);
                }
                m_pendingReplies[rep].second(rep);
            }

            if (done)
                *done = true;

            m_pendingReplies.remove(rep);
        },
        Qt::DirectConnection);

    if (!async)
        while (!(*done))
        {
            qApp->processEvents();
            qApp->sendPostedEvents();
        }

    if (done)
        delete done;
}
