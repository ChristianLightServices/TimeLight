#include "AbstractTimeServiceManager.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include "JsonHelper.h"
#include "Project.h"
#include "TimeEntry.h"
#include "User.h"
#include "Workspace.h"

AbstractTimeServiceManager::AbstractTimeServiceManager(const QByteArray &apiKey, QObject *parent)
    : QObject{parent},
      m_apiKey{apiKey}
{
    m_manager.setAutoDeleteReplies(true);

    m_expireProjectsTimer.setInterval(15 * 60 * 1000);
    m_expireProjectsTimer.callOnTimeout(this, [this] {
        m_projectsLoaded = false;
        m_projects.clear();
    });
    m_expireProjectsTimer.setSingleShot(true);

    m_expireWorkspacesTimer.setInterval(15 * 60 * 1000); // every 15 mins
    m_expireWorkspacesTimer.callOnTimeout(this, [this] {
        m_workspacesLoaded = false;
        m_workspaces.clear();
    });
    m_expireWorkspacesTimer.setSingleShot(true);

    m_expireUsersTimer.setInterval(15 * 60 * 1000); // every 15 mins
    m_expireUsersTimer.callOnTimeout(this, [this] {
        m_usersLoaded = false;
        m_users.clear();
    });
    m_expireUsersTimer.setSingleShot(true);

    // We won't actually populate the user or project list yet; why do it when we probably won't need it for a long time?
}

QVector<Project> &AbstractTimeServiceManager::projects()
{
    if (!m_projectsLoaded)
        updateProjects();

    return m_projects;
}

QVector<Workspace> &AbstractTimeServiceManager::workspaces()
{
    if (!m_workspacesLoaded)
        updateWorkspaces();

    return m_workspaces;
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

void AbstractTimeServiceManager::startTimeEntry(const QString &userId, const Project &project, bool async)
{
    startTimeEntry(userId, project, currentDateTime(), async);
}

void AbstractTimeServiceManager::startTimeEntry(const QString &userId,
                                                const Project &project,
                                                const QDateTime &start,
                                                bool async)
{
    TimeEntry t{{}, project, userId, start, {}, {}};
    timeEntryReq(startTimeEntryUrl(userId, m_workspaceId),
                 TimeEntryAction::StartTimeEntry,
                 QByteArray::fromStdString(timeEntryToJson(t, TimeEntryAction::StartTimeEntry).dump()),
                 async);
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

void AbstractTimeServiceManager::modifyTimeEntry(const QString &userId, const TimeEntry &t, bool async)
{
    timeEntryReq(modifyTimeEntryUrl(userId, m_workspaceId, t.id()),
                 TimeEntryAction::ModifyTimeEntry,
                 QByteArray::fromStdString(timeEntryToJson(t, TimeEntryAction::ModifyTimeEntry).dump()),
                 async);
}

QVector<TimeEntry> AbstractTimeServiceManager::getTimeEntries(const QString &userId,
                                                              [[maybe_unused]] std::optional<int> pageNumber,
                                                              [[maybe_unused]] std::optional<int> pageSize,
                                                              std::optional<QDateTime> start,
                                                              std::optional<QDateTime> end)
{
    QVector<TimeEntry> entries;

    auto url = timeEntriesUrl(userId, workspaceId(), start, end);
    auto worker = [this, &url, &entries, &pageSize]() -> bool {
        bool retVal;
        get(url, false, [this, &retVal, &entries, &pageSize](QNetworkReply *rep) {
            try
            {
                json j{json::parse(rep->readAll().toStdString())};

                if (j.is_array() && j[0].is_array())
                    j = j[0];
                if (!j.is_null() && !j[0].is_null())
                {
                    entries.resize(j.size());
                    using namespace std::placeholders;
                    std::transform(j.begin(),
                                   j.end(),
                                   entries.begin(),
                                   std::bind(&AbstractTimeServiceManager::jsonToTimeEntry, this, _1));
                }

                if (timeEntriesSortOrder() == Qt::AscendingOrder)
                    std::reverse(entries.begin(), entries.end());
                retVal = !j.empty() && entries.size() == pageSize.value_or(timeEntriesPaginationPageSize());
            }
            catch (const std::exception &e)
            {
                logger()->error("Error in parsing time entries: {}", e.what());
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
            QUrlQuery query{url};
            query.removeAllQueryItems(timeEntriesPageSizeHeaderName());
            query.removeAllQueryItems(timeEntriesPageHeaderName());
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

void AbstractTimeServiceManager::deleteTimeEntry(const QString &userId, const TimeEntry &t, bool async)
{
    timeEntryReq(deleteTimeEntryUrl(userId, m_workspaceId, t.id()),
                 TimeEntryAction::DeleteTimeEntry,
                 QByteArray::fromStdString(timeEntryToJson(t, TimeEntryAction::DeleteTimeEntry).dump()),
                 async);
}

User AbstractTimeServiceManager::getApiKeyOwner()
{
    json j;
    get(currentUserUrl(), false, [this, &j](QNetworkReply *rep) {
        try
        {
            j = json::parse(rep->readAll().toStdString());
        }
        catch (const std::exception &e)
        {
            logger()->error("Could not parse user JSON: {}", e.what());
        }
    });

    return j.empty() ? User{this} : jsonToUser(j);
}

Workspace AbstractTimeServiceManager::currentWorkspace()
{
    // By calling workspaces(), we ensure that the loading logic will happen if needed.
    auto it =
        std::find_if(workspaces().begin(), workspaces().end(), [this](const auto &w) { return w.id() == m_workspaceId; });
    if (it == workspaces().end())
        return {this};
    else
        return *it;
}

void AbstractTimeServiceManager::setApiKey(const QString &apiKey)
{
    m_apiKey = apiKey.toUtf8();
    updateCurrentUser();
    emit apiKeyChanged();
}

void AbstractTimeServiceManager::setWorkspaceId(const QString &workspaceId)
{
    if (std::find_if(workspaces().begin(), workspaces().end(), [&workspaceId](const auto &w) {
            return w.id() == workspaceId;
        }) == workspaces().end())
        return;

    m_workspaceId = workspaceId;
    m_projectsLoaded = false;
    m_usersLoaded = false;
}

void AbstractTimeServiceManager::setLogger(std::shared_ptr<spdlog::logger> newLogger)
{
    m_logger = newLogger ? newLogger : std::make_shared<spdlog::logger>(serviceIdentifier().toStdString());
}

void AbstractTimeServiceManager::callInitVirtualMethods()
{
    // request currently logged in user (the one whose API key we're using) as a validity test
    // and also in order to cache API key info
    updateCurrentUser();

    // set up logger
    setLogger(nullptr);
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
            catch (const std::exception &e)
            {
                logger()->error("Error while parsing user: {}", e.what());
            }
        },
        [this](QNetworkReply *rep) {
            if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == 0) [[likely]]
                m_isValid = true; // hopefully the connection will work once the internet is connected
            defaultFailureCb(rep);
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

                auto appendPos = m_users.size();
                m_users.resize(appendPos + j.size());
                using namespace std::placeholders;
                std::transform(j.begin(),
                               j.end(),
                               m_users.begin() + appendPos,
                               std::bind(&AbstractTimeServiceManager::jsonToUserData, this, _1));

                retCode = !j.empty();
            }
            catch (const std::exception &e)
            {
                logger()->error("Error in parsing users: {}", e.what());
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
            QUrlQuery query{url};
            query.removeAllQueryItems(usersPageSizeHeaderName());
            query.removeAllQueryItems(usersPageHeaderName());
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

                // I'm not sure why, but nlohmann::json tends to make everything into an array
                if (j.is_array() && j[0].is_array())
                    j = j[0];

                auto appendPos = m_projects.size();
                m_projects.resize(appendPos + j.size());
                using namespace std::placeholders;
                std::transform(j.begin(),
                               j.end(),
                               m_projects.begin() + appendPos,
                               std::bind(&AbstractTimeServiceManager::jsonToProject, this, _1));

                retVal = !j.empty() && !j.is_null();
            }
            catch (const std::exception &e)
            {
                logger()->error("Error in parsing projects: {}", e.what());
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
            QUrlQuery query{url};
            query.removeAllQueryItems(projectsPageSizeHeaderName());
            query.removeAllQueryItems(projectsPageHeaderName());
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

void AbstractTimeServiceManager::updateWorkspaces()
{
    if (m_loadingWorkspaces)
        return;

    m_loadingWorkspaces = true;
    m_workspacesLoaded = false;
    m_workspaces.clear();

    auto url = workspacesUrl();
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

                if (j.is_array() && j[0].is_array())
                    j = j[0];

                auto appendPos = m_workspaces.size();
                m_workspaces.resize(appendPos + j.size());
                using namespace std::placeholders;
                std::transform(j.begin(),
                               j.end(),
                               m_workspaces.begin() + appendPos,
                               std::bind(&AbstractTimeServiceManager::jsonToWorkspace, this, _1));

                retVal = !j.empty() && !j.is_null();
            }
            catch (const std::exception &e)
            {
                logger()->error("Error in parsing workspaces: {}", e.what());
                retVal = false;
            }
        });
        return retVal;
    };

    if (supportedPagination().testFlag(Pagination::Workspaces))
    {
        int page = paginationStartsAt();

        do
        {
            QUrlQuery query{url};
            query.removeAllQueryItems(workspacesPageSizeHeaderName());
            query.removeAllQueryItems(workspacesPageHeaderName());
            query.addQueryItem(workspacesPageSizeHeaderName(), QString::number(workspacesPaginationPageSize()));
            query.addQueryItem(workspacesPageHeaderName(), QString::number(page++));
            url.setQuery(query);
        } while (worker());
    }
    else
        worker();

    m_workspacesLoaded = true;
    m_loadingWorkspaces = false;
    m_expireWorkspacesTimer.start();
}

void AbstractTimeServiceManager::timeEntryReq(const QUrl &url,
                                              const TimeEntryAction action,
                                              const QByteArray &body,
                                              bool async,
                                              NetworkReplyCallback successCb,
                                              NetworkReplyCallback failureCb)
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
    else if (verb == HttpVerb::Delete)
        del(url, body, async, httpReturnCodeForVerb(verb), successCb, failureCb);
}

QDateTime AbstractTimeServiceManager::jsonToDateTime(const nlohmann::json &j) const
{
    try
    {
        return QDateTime::fromString(j.get<QString>(), jsonTimeFormatString());
    }
    catch (const std::exception &e)
    {
        logger()->error("Error in parsing time: {}", e.what());
        return {};
    }
}

json AbstractTimeServiceManager::dateTimeToJson(const QDateTime &dt) const
{
    return dt.toString(jsonTimeFormatString());
}

void AbstractTimeServiceManager::defaultSuccessCb(QNetworkReply *rep) {}

void AbstractTimeServiceManager::defaultFailureCb(QNetworkReply *reply)
{
    if (auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); code != 0)
    {
        logger()->error("Request {} failed with code {}", reply->url().toString().toStdString(), code);
        logger()->debug("Response data: '{}'", reply->readAll());
    }
}

// *****************************************************************************
// *****     BEYOND THIS POINT THERE ARE ONLY FUNCTIONS FOR REST STUFF     *****
// *****************************************************************************

void AbstractTimeServiceManager::get(const QUrl &url, NetworkReplyCallback successCb, NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Get, url, {}, true, httpReturnCodeForVerb(HttpVerb::Get), successCb, failureCb);
}

void AbstractTimeServiceManager::get(const QUrl &url,
                                     int expectedReturnCode,
                                     NetworkReplyCallback successCb,
                                     NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Get, url, {}, true, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::get(const QUrl &url,
                                     bool async,
                                     NetworkReplyCallback successCb,
                                     NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Get, url, {}, async, httpReturnCodeForVerb(HttpVerb::Get), successCb, failureCb);
}

void AbstractTimeServiceManager::get(
    const QUrl &url, bool async, int expectedReturnCode, NetworkReplyCallback successCb, NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Get, url, {}, async, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::post(const QUrl &url,
                                      const QByteArray &body,
                                      NetworkReplyCallback successCb,
                                      NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Post, url, body, true, httpReturnCodeForVerb(HttpVerb::Post), successCb, failureCb);
}

void AbstractTimeServiceManager::post(const QUrl &url,
                                      const QByteArray &body,
                                      int expectedReturnCode,
                                      NetworkReplyCallback successCb,
                                      NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Post, url, body, true, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::post(
    const QUrl &url, const QByteArray &body, bool async, NetworkReplyCallback successCb, NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Post, url, body, async, httpReturnCodeForVerb(HttpVerb::Post), successCb, failureCb);
}

void AbstractTimeServiceManager::post(const QUrl &url,
                                      const QByteArray &body,
                                      bool async,
                                      int expectedReturnCode,
                                      NetworkReplyCallback successCb,
                                      NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Post, url, body, async, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::patch(const QUrl &url,
                                       const QByteArray &body,
                                       NetworkReplyCallback successCb,
                                       NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Patch, url, body, true, httpReturnCodeForVerb(HttpVerb::Patch), successCb, failureCb);
}

void AbstractTimeServiceManager::patch(const QUrl &url,
                                       const QByteArray &body,
                                       int expectedReturnCode,
                                       NetworkReplyCallback successCb,
                                       NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Patch, url, body, true, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::patch(
    const QUrl &url, const QByteArray &body, bool async, NetworkReplyCallback successCb, NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Patch, url, body, async, httpReturnCodeForVerb(HttpVerb::Patch), successCb, failureCb);
}

void AbstractTimeServiceManager::patch(const QUrl &url,
                                       const QByteArray &body,
                                       bool async,
                                       int expectedReturnCode,
                                       NetworkReplyCallback successCb,
                                       NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Patch, url, body, async, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::put(const QUrl &url,
                                     const QByteArray &body,
                                     NetworkReplyCallback successCb,
                                     NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Put, url, body, true, httpReturnCodeForVerb(HttpVerb::Put), successCb, failureCb);
}

void AbstractTimeServiceManager::put(const QUrl &url,
                                     const QByteArray &body,
                                     int expectedReturnCode,
                                     NetworkReplyCallback successCb,
                                     NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Put, url, body, true, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::put(
    const QUrl &url, const QByteArray &body, bool async, NetworkReplyCallback successCb, NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Put, url, body, async, httpReturnCodeForVerb(HttpVerb::Put), successCb, failureCb);
}

void AbstractTimeServiceManager::put(const QUrl &url,
                                     const QByteArray &body,
                                     bool async,
                                     int expectedReturnCode,
                                     NetworkReplyCallback successCb,
                                     NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Put, url, body, async, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::head(const QUrl &url, NetworkReplyCallback successCb, NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Head, url, {}, true, httpReturnCodeForVerb(HttpVerb::Head), successCb, failureCb);
}

void AbstractTimeServiceManager::head(const QUrl &url,
                                      int expectedReturnCode,
                                      NetworkReplyCallback successCb,
                                      NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Head, url, {}, true, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::head(const QUrl &url,
                                      bool async,
                                      NetworkReplyCallback successCb,
                                      NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Head, url, {}, async, httpReturnCodeForVerb(HttpVerb::Head), successCb, failureCb);
}

void AbstractTimeServiceManager::head(
    const QUrl &url, bool async, int expectedReturnCode, NetworkReplyCallback successCb, NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Head, url, {}, async, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::del(const QUrl &url,
                                     const QByteArray &body,
                                     NetworkReplyCallback successCb,
                                     NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Delete, url, body, true, httpReturnCodeForVerb(HttpVerb::Delete), successCb, failureCb);
}

void AbstractTimeServiceManager::del(const QUrl &url,
                                     const QByteArray &body,
                                     int expectedReturnCode,
                                     NetworkReplyCallback successCb,
                                     NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Delete, url, body, true, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::del(
    const QUrl &url, const QByteArray &body, bool async, NetworkReplyCallback successCb, NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Delete, url, body, async, httpReturnCodeForVerb(HttpVerb::Delete), successCb, failureCb);
}

void AbstractTimeServiceManager::del(const QUrl &url,
                                     const QByteArray &body,
                                     bool async,
                                     int expectedReturnCode,
                                     NetworkReplyCallback successCb,
                                     NetworkReplyCallback failureCb)
{
    httpRequest(HttpVerb::Delete, url, body, async, expectedReturnCode, successCb, failureCb);
}

void AbstractTimeServiceManager::httpRequest(const HttpVerb verb,
                                             const QUrl &url,
                                             const QByteArray &body,
                                             bool async,
                                             int expectedReturnCode,
                                             NetworkReplyCallback successCb,
                                             NetworkReplyCallback failureCb)
{
    QNetworkRequest req{url};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader(authHeaderName(), apiKeyForRequests());

    QNetworkReply *rep = nullptr;
    switch (verb)
    {
    case HttpVerb::Get:
        rep = m_manager.get(req);
        break;
    case HttpVerb::Post:
        rep = m_manager.post(req, body);
        break;
    case HttpVerb::Patch:
        rep = m_manager.sendCustomRequest(req, QByteArrayLiteral("PATCH"), body);
        break;
    case HttpVerb::Put:
        rep = m_manager.put(req, body);
        break;
    case HttpVerb::Head:
        rep = m_manager.head(req);
        break;
    case HttpVerb::Delete:
        rep = m_manager.deleteResource(req);
        break;
    default:
        break;
    }

    using namespace std::placeholders;
    m_pendingReplies.insert(rep,
                            {successCb ? successCb : std::bind(&AbstractTimeServiceManager::defaultSuccessCb, this, _1),
                             failureCb ? failureCb : std::bind(&AbstractTimeServiceManager::defaultFailureCb, this, _1)});

    // The reason that not all of the event loop code is located together is that, in my experience so far, the loop has to
    // be connected to finished() before the reply handler is connected.
    QEventLoop *loop;
    if (!async)
    {
        loop = new QEventLoop{this};
        connect(rep, &QNetworkReply::finished, loop, &QEventLoop::quit);
    }

    connect(
        rep,
        &QNetworkReply::finished,
        this,
        [this, rep, expectedReturnCode] {
            auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (status == expectedReturnCode) [[likely]]
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
                if (status == 429)
                {
                    m_isRatelimited = true;
                    emit ratelimited(true);
                }
                m_pendingReplies[rep].second(rep);
            }

            // offline?
            if (status == 0)
            {
                if (m_isConnectedToInternet)
                {
                    logger()->warn("Internet connection lost");
                    m_isConnectedToInternet = false;
                    emit internetConnectionChanged(false);
                }
            }
            else
            {
                if (!m_isConnectedToInternet)
                {
                    logger()->info("Internet connection restored");
                    m_isConnectedToInternet = true;
                    emit internetConnectionChanged(true);
                }
            }

            m_pendingReplies.remove(rep);
        },
        Qt::DirectConnection);

    if (!async)
        loop->exec();
}
