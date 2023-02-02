#include "TimeCampManager.h"

#include <QUrlQuery>

#include "User.h"

TimeCampManager::TimeCampManager(const QByteArray &apiKey, QObject *parent)
    : AbstractTimeServiceManager{apiKey, parent}
{
    callInitVirtualMethods();
    // TimeCamp seems to like to fail on the first fetch, so we'll make a fetch now
    getApiKeyOwner();
}

QUrl TimeCampManager::runningTimeEntryUrl(const QString &userId, const QString &workspaceId)
{
    Q_UNUSED(userId)
    Q_UNUSED(workspaceId)
    return {apiBaseUrl() + "/timer"};
}

QUrl TimeCampManager::startTimeEntryUrl(const QString &userId, const QString &workspaceId)
{
    return {apiBaseUrl() + "/timer"};
}

QUrl TimeCampManager::stopTimeEntryUrl(const QString &userId, const QString &workspaceId)
{
    Q_UNUSED(userId)
    Q_UNUSED(workspaceId)
    return {apiBaseUrl() + "/timer"};
}

QUrl TimeCampManager::modifyTimeEntryUrl(const QString &userId, const QString &workspaceId, const QString &timeEntryId)
{
    Q_UNUSED(userId)
    Q_UNUSED(workspaceId)
    return {apiBaseUrl() + "/entries"};
}

QUrl TimeCampManager::deleteTimeEntryUrl(const QString &userId, const QString &workspaceId, const QString &timeEntryId)
{
    Q_UNUSED(userId)
    Q_UNUSED(workspaceId)
    Q_UNUSED(timeEntryId)
    return {apiBaseUrl() + "/timer"};
}

QUrl TimeCampManager::timeEntryUrl(const QString &userId, const QString &workspaceId, const QString &timeEntryId)
{
    throw std::logic_error{"TimeCamp doesn't yet support fetching time entries!"};
}

QUrl TimeCampManager::timeEntriesUrl(const QString &userId,
                                     const QString &workspaceId,
                                     std::optional<QDateTime> start,
                                     std::optional<QDateTime> end) const
{
    QUrl url{apiBaseUrl() + "/entries"};
    QUrlQuery q;
    q.addQueryItem("user_ids", "me");
    auto date = QDate::currentDate();
    // The past 2 months should be sufficient if a date is not set. I do not like TimeCamp's "pagination".
    q.addQueryItem("from", (start ? start->toLocalTime().date() : date.addDays(-1)).toString(QStringLiteral("yyyy-MM-dd")));
    q.addQueryItem("to", (end ? end->toLocalTime().date() : date).toString(QStringLiteral("yyyy-MM-dd")));
    url.setQuery(q);
    return url;
}

QUrl TimeCampManager::currentUserUrl() const
{
    return {apiBaseUrl() + "/me"};
}

QUrl TimeCampManager::workspacesUrl() const
{
    return {apiBaseUrl() + "/group"};
}

QUrl TimeCampManager::usersUrl(const QString &workspaceId) const
{
    return {apiBaseUrl() + "/group/" + workspaceId + "/user"};
}

QUrl TimeCampManager::projectsUrl(const QString &workspaceId) const
{
    return {apiBaseUrl() + "/tasks"};
}

const QFlags<AbstractTimeServiceManager::Pagination> TimeCampManager::supportedPagination() const
{
    return {};
}

std::optional<TimeEntry> TimeCampManager::jsonToRunningTimeEntry(const nlohmann::json &j)
{
    try
    {
        if (!j["isTimerRunning"].get<bool>())
            return std::nullopt;

        return TimeEntry{j["entry_id"].get<QString>(),
                         getProjectById(j["task_id"].get<QString>()),
                         getApiKeyOwner()->userId(),
                         jsonToDateTime(j["start_time"]),
                         {},
                         true,
                         {{"timer_id", j["timer_id"]}},
                         this};
    }
    catch (const std::exception &e)
    {
        logger()->error("Error while parsing running time entry: {}", e.what());
        return std::nullopt;
    }
}

TimeEntry TimeCampManager::jsonToTimeEntry(const nlohmann::json &j)
{
    try
    {
        auto day = QDate::fromString(j["date"].get<QString>(), QStringLiteral("yyyy-MM-dd"));
        auto project = getProjectById(j["task_id"].get<QString>());
        if (j.contains("description"))
            project.setDescription(j["description"].get<QString>());

        return TimeEntry{
            QString::number(j["id"].get<int>()),
            project,
            j["user_id"].get<QString>(),
            QDateTime{day, QTime::fromString(j["start_time"].get<QString>(), QStringLiteral("HH:mm:ss")), Qt::LocalTime},
            QDateTime{day, QTime::fromString(j["end_time"].get<QString>(), QStringLiteral("HH:mm:ss")), Qt::LocalTime},
            false,
            {},
            this};
    }
    catch (const std::exception &e)
    {
        logger()->error("Error while parsing time entry: {}", e.what());
        return {this};
    }
}

QSharedPointer<User> TimeCampManager::jsonToUser(const nlohmann::json &j)
{
    try
    {
        return QSharedPointer<User>::create(j["user_id"].get<QString>(),
                                            j["display_name"].get<QString>(),
                                            j["root_group_id"].get<QString>(),
                                            QString{},
                                            j["email"].get<QString>(),
                                            sharedFromThis());
    }
    catch (const std::exception &e)
    {
        logger()->error("Error while parsing user: {}", e.what());
        return QSharedPointer<User>::create(this);
    }
}

QPair<QString, QString> TimeCampManager::jsonToUserData(const nlohmann::json &j)
{
    try
    {
        return {j["id"].get<QString>(), (j.contains("display_name") ? j["display_name"] : j["email"]).get<QString>()};
    }
    catch (const std::exception &e)
    {
        logger()->error("Error while parsing users: {}", e.what());
        return {};
    }
}

Workspace TimeCampManager::jsonToWorkspace(const nlohmann::json &j)
{
    try
    {
        auto val = (j.is_array() ? j[0] : j);
        return Workspace{val["group_id"].get<QString>(), val["name"].get<QString>(), this};
    }
    catch (const std::exception &e)
    {
        logger()->error("Error while parsing workspace: {}", e.what());
        return {this};
    }
}

Project TimeCampManager::jsonToProject(const nlohmann::json &j)
{
    try
    {
        auto val = (j.is_array() ? j[0] : j);
        return Project{QString::number(val["task_id"].get<int>()),
                       val["name"].get<QString>(),
                       static_cast<bool>(val["archived"].get<int>()),
                       this};
    }
    catch (const std::exception &e)
    {
        logger()->error("Error while parsing project: {}", e.what());
        return {this};
    }
}

json TimeCampManager::timeEntryToJson(const TimeEntry &t, TimeEntryAction action)
{
    if (action == TimeEntryAction::StopTimeEntry)
        return {{"action", "stop"}, {"stopped_at", dateTimeToJson(t.end())}};
    else if (action == TimeEntryAction::StartTimeEntry)
        return {{"action", "start"}, {"started_at", dateTimeToJson(t.start())}, {"task_id", t.project().id()}};
    else if (action == TimeEntryAction::ModifyTimeEntry)
    {
        json j = {{"id", t.id()},
                  {"start_time", t.start().time().toString(QStringLiteral("HH:mm:ss"))},
                  {"task_id", t.project().id()},
                  {"description", t.project().description()}};
        if (!t.end().isNull())
        {
            QDateTime endTime{t.end()};
            if (t.end().date() > t.start().date())
                endTime = QDateTime{t.start().date().endOfDay()};
            j["end_time"] = endTime.toLocalTime().time().toString(QStringLiteral("HH:mm:ss"));
        }
        return j;
    }
    else if (action == TimeEntryAction::DeleteTimeEntry)
        return {{"timer_id", t.extraData["timer_id"]}};
    else
        return {};
}

AbstractTimeServiceManager::HttpVerb TimeCampManager::httpVerbForAction(const TimeEntryAction action) const
{
    switch (action)
    {
    case TimeEntryAction::GetRunningTimeEntry:
    case TimeEntryAction::StartTimeEntry:
    case TimeEntryAction::StopTimeEntry:
        return HttpVerb::Post;
    case TimeEntryAction::ModifyTimeEntry:
        return HttpVerb::Put;
    case TimeEntryAction::DeleteTimeEntry:
        return HttpVerb::Delete;
    default:
        logger()->error("Unhandled time entry action: {}:{}", __FILE__, __LINE__);
        Q_UNREACHABLE();
        return HttpVerb::Post;
    }
}

int TimeCampManager::httpReturnCodeForVerb(const HttpVerb verb) const
{
    switch (verb)
    {
    case HttpVerb::Get:
    case HttpVerb::Patch:
    case HttpVerb::Head:
    case HttpVerb::Post:
    case HttpVerb::Put:
    case HttpVerb::Delete:
        return 200;
    default:
        return -1;
    }
}
