#include "ClockifyManager.h"

#include <QUrlQuery>

#include "TimeEntry.h"
#include "User.h"

ClockifyManager::ClockifyManager(const QByteArray &apiKey, QObject *parent)
    : AbstractTimeServiceManager{apiKey, parent}
{
    callInitVirtualMethods();
}

QUrl ClockifyManager::runningTimeEntryUrl(const QString &userId, const QString &workspaceId)
{
    auto url = timeEntriesUrl(userId, workspaceId);
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("in-progress"), QStringLiteral("true"));
    url.setQuery(q);
    return url;
}

QUrl ClockifyManager::startTimeEntryUrl(const QString &userId, const QString &workspaceId)
{
    return timeEntriesUrl(userId, workspaceId);
}

QUrl ClockifyManager::stopTimeEntryUrl(const QString &userId, const QString &workspaceId)
{
    return timeEntriesUrl(userId, workspaceId);
}

QUrl ClockifyManager::modifyTimeEntryUrl(const QString &userId, const QString &workspaceId, const QString &timeEntryId)
{
    return timeEntryUrl(userId, workspaceId, timeEntryId);
}

QUrl ClockifyManager::deleteTimeEntryUrl(const QString &userId, const QString &workspaceId, const QString &timeEntryId)
{
    return timeEntryUrl(userId, workspaceId, timeEntryId);
}

QUrl ClockifyManager::timeEntryUrl([[maybe_unused]] const QString &userId,
                                   const QString &workspaceId,
                                   const QString &timeEntryId)
{
    return {apiBaseUrl() + "/workspaces/" + workspaceId + "/time-entries/" + timeEntryId};
}

QUrl ClockifyManager::timeEntriesUrl(const QString &userId,
                                     const QString &workspaceId,
                                     std::optional<QDateTime> start,
                                     std::optional<QDateTime> end) const
{
    QUrl url{apiBaseUrl() + "/workspaces/" + workspaceId + "/user/" + userId + "/time-entries"};
    QUrlQuery q;
    if (start)
        q.addQueryItem(QStringLiteral("start"), start->toString(jsonTimeFormatString()));
    if (end)
        q.addQueryItem(QStringLiteral("end"), end->toString(jsonTimeFormatString()));
    url.setQuery(q);
    return url;
}

QUrl ClockifyManager::currentUserUrl() const
{
    return {apiBaseUrl() + "/user"};
}

QUrl ClockifyManager::workspacesUrl() const
{
    return {apiBaseUrl() + "/workspaces"};
}

QUrl ClockifyManager::usersUrl(const QString &workspaceId) const
{
    return {apiBaseUrl() + "/workspaces/" + workspaceId + "/users"};
}

QUrl ClockifyManager::projectsUrl(const QString &workspaceId) const
{
    return {apiBaseUrl() + "/workspaces/" + workspaceId + "/projects"};
}

const QFlags<AbstractTimeServiceManager::Pagination> ClockifyManager::supportedPagination() const
{
    QFlags<Pagination> f;
    f.setFlag(Pagination::Projects);
    f.setFlag(Pagination::Users);
    f.setFlag(Pagination::TimeEntries);
    return f;
}

std::optional<TimeEntry> ClockifyManager::jsonToRunningTimeEntry(const nlohmann::json &j)
{
    auto e{jsonToTimeEntry(j)};
    return e.isValid() ? std::optional{e} : std::nullopt;
}

TimeEntry ClockifyManager::jsonToTimeEntry(const nlohmann::json &j)
{
    try
    {
        if (j.empty())
            return {this};

        auto entry = (j.is_array() ? j[0] : j);

        auto id = entry["id"].get<QString>();
        Project project;
        if (entry.contains("projectId") && !entry["projectId"].is_null())
        {
            QString projectId;
            projectId = entry["projectId"].get<QString>();
            project = {projectId,
                       projectName(projectId),
                       entry.contains("description") ? entry["description"].get<QString>() : QString{}};
        }
        auto userId = entry["userId"].get<QString>();
        auto start = jsonToDateTime(entry["timeInterval"]["start"]);
        QDateTime end;
        std::optional<bool> running;
        if (entry["timeInterval"].contains("end") && !entry["timeInterval"]["end"].is_null())
        {
            end = jsonToDateTime(entry["timeInterval"]["end"]);
            running = false;
        }
        else
            running = true;

        start.setTimeSpec(Qt::UTC);
        end.setTimeSpec(Qt::UTC);
        return TimeEntry{id, project, userId, start, end, running, {}, this};
    }
    catch (const std::exception &e)
    {
        logger()->error("Error while creating time entry: {}", e.what());
        return {this};
    }
}

QSharedPointer<User> ClockifyManager::jsonToUser(const nlohmann::json &j)
{
    try
    {
        return QSharedPointer<User>::create(j["id"].get<QString>(),
                                            j["name"].get<QString>(),
                                            j["defaultWorkspace"].get<QString>(),
                                            j["profilePicture"].get<QString>(),
                                            j["email"].get<QString>(),
                                            sharedFromThis());
    }
    catch (const std::exception &e)
    {
        logger()->error("Error while parsing user: {}", e.what());
        return QSharedPointer<User>::create(this);
    }
}

QPair<QString, QString> ClockifyManager::jsonToUserData(const nlohmann::json &j)
{
    try
    {
        return {j["id"].get<QString>(), (j.contains("name") ? j["name"] : j["email"]).get<QString>()};
    }
    catch (const std::exception &e)
    {
        logger()->error("Error while parsing users: {}", e.what());
        return {};
    }
}

Workspace ClockifyManager::jsonToWorkspace(const nlohmann::json &j)
{
    try
    {
        auto val = (j.is_array() ? j[0] : j);
        return Workspace{val["id"].get<QString>(), val["name"].get<QString>(), this};
    }
    catch (const std::exception &e)
    {
        logger()->error("Error while parsing workspace: {}", e.what());
        return {this};
    }
}

Project ClockifyManager::jsonToProject(const nlohmann::json &j)
{
    try
    {
        auto val = (j.is_array() ? j[0] : j);
        return Project{val["id"].get<QString>(), val["name"].get<QString>(), this};
    }
    catch (const std::exception &e)
    {
        logger()->error("Error while parsing project: {}", e.what());
        return {this};
    }
}

json ClockifyManager::timeEntryToJson(const TimeEntry &t, TimeEntryAction action)
{
    if (action == TimeEntryAction::DeleteTimeEntry)
        return {};

    json j;
    j["start"] = dateTimeToJson(t.start().toUTC());
    if (!t.end().isNull())
        j["end"] = dateTimeToJson(t.end().toUTC());
    if (!t.project().description().isEmpty())
        j["description"] = t.project().description();
    j["projectId"] = t.project().id();
    return j;
}

AbstractTimeServiceManager::HttpVerb ClockifyManager::httpVerbForAction(const TimeEntryAction action) const
{
    switch (action)
    {
    case TimeEntryAction::GetRunningTimeEntry:
        return HttpVerb::Get;
    case TimeEntryAction::StartTimeEntry:
        return HttpVerb::Post;
    case TimeEntryAction::StopTimeEntry:
        return HttpVerb::Patch;
    case TimeEntryAction::ModifyTimeEntry:
        return HttpVerb::Put;
    case TimeEntryAction::DeleteTimeEntry:
        return HttpVerb::Delete;
    default:
        logger()->error("Unhandled time entry action: {}:{}", __FILE__, __LINE__);
        Q_UNREACHABLE();
        return HttpVerb::Get;
    }
}

int ClockifyManager::httpReturnCodeForVerb(const HttpVerb verb) const
{
    switch (verb)
    {
    case HttpVerb::Get:
    case HttpVerb::Patch:
    case HttpVerb::Head:
    case HttpVerb::Put:
        return 200;
    case HttpVerb::Post:
        return 201;
    case HttpVerb::Delete:
        return 204;
    default:
        // for unused verbs
        return -1;
    }
}
