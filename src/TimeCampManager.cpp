#include "TimeCampManager.h"

#include <QUrlQuery>

#include "User.h"

TimeCampManager::TimeCampManager(const QByteArray &apiKey, QObject *parent) : AbstractTimeServiceManager{apiKey, parent}
{
    callInitVirtualMethods();
    // TimeCamp seems to like to fail on the first fetch, so we'll make a fetch now
    getApiKeyOwner();
}

QUrl TimeCampManager::runningTimeEntryUrl(const QString &userId, const QString &workspaceId)
{
    Q_UNUSED(userId)
    Q_UNUSED(workspaceId)
    return {baseUrl() + "/timer"};
}

QUrl TimeCampManager::startTimeEntryUrl(const QString &userId, const QString &workspaceId)
{
    return {baseUrl() + "/timer"};
}

QUrl TimeCampManager::stopTimeEntryUrl(const QString &userId, const QString &workspaceId)
{
    Q_UNUSED(userId)
    Q_UNUSED(workspaceId)
    return {baseUrl() + "/timer"};
}

QUrl TimeCampManager::modifyTimeEntryUrl(const QString &userId, const QString &workspaceId, const QString &timeEntryId)
{
    Q_UNUSED(userId)
    Q_UNUSED(workspaceId)
    return {baseUrl() + "/entries"};
}

QUrl TimeCampManager::deleteTimeEntryUrl(const QString &userId, const QString &workspaceId, const QString &timeEntryId)
{
    Q_UNUSED(userId)
    Q_UNUSED(workspaceId)
    Q_UNUSED(timeEntryId)
    return {baseUrl() + "/timer"};
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
    QUrl url{baseUrl() + "/entries"};
    QUrlQuery q;
    q.addQueryItem("user_ids", "me");
    auto date = QDate::currentDate();
    // The past 2 months should be sufficient if a date is not set. I do not like TimeCamp's "pagination".
    q.addQueryItem("from", (start ? start->date() : date.addDays(-1)).toString("yyyy-MM-dd"));
    q.addQueryItem("to", (end ? end->date() : date).toString("yyyy-MM-dd"));
    url.setQuery(q);
    return url;
}

QUrl TimeCampManager::currentUserUrl() const
{
    return {baseUrl() + "/me"};
}

QUrl TimeCampManager::workspacesUrl() const
{
    return {baseUrl() + "/group"};
}

QUrl TimeCampManager::usersUrl(const QString &workspaceId) const
{
    return {baseUrl() + "/group/" + workspaceId + "/user"};
}

QUrl TimeCampManager::projectsUrl(const QString &workspaceId) const
{
    return {baseUrl() + "/tasks"};
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
                         Project{{j["task_id"].get<QString>()}, {j["name"].get<QString>()}, this},
                         getApiKeyOwner().userId(),
                         jsonToDateTime(j["start_time"]),
                         {},
                         true,
                         {{"timer_id", j["timer_id"]}},
                         this};
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error while parsing running time entry: " << e.what() << std::endl;
        return std::nullopt;
    }
}

TimeEntry TimeCampManager::jsonToTimeEntry(const nlohmann::json &j)
{
    try
    {
        auto day = QDate::fromString(j["date"].get<QString>(), QStringLiteral("yyyy-MM-dd"));
        return TimeEntry{QString::number(j["id"].get<int>()),
                         Project{j["task_id"].get<QString>(),
                                 j["name"].get<QString>(),
                                 j.contains("description") ? j["description"].get<QString>() : QString{},
                                 this},
                         j["user_id"].get<QString>(),
                         QDateTime{day, QTime::fromString(j["start_time"].get<QString>(), QStringLiteral("HH:mm:ss"))},
                         QDateTime{day, QTime::fromString(j["end_time"].get<QString>(), QStringLiteral("HH:mm:ss"))},
                         false,
                         {},
                         this};
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error while parsing time entry: " << e.what() << std::endl;
        return {this};
    }
}

User TimeCampManager::jsonToUser(const nlohmann::json &j)
{
    try
    {
        return User{j["user_id"].get<QString>(), j["display_name"].get<QString>(), j["root_group_id"].get<QString>(), this};
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error while parsing user: " << e.what() << std::endl;
        return {this};
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
        std::cerr << "Error while parsing users: " << e.what() << std::endl;
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
        std::cerr << "Error while parsing workspace: " << e.what() << std::endl;
        return {this};
    }
}

Project TimeCampManager::jsonToProject(const nlohmann::json &j)
{
    try
    {
        auto val = (j.is_array() ? j[0] : j);
        return Project{QString::number(val["task_id"].get<int>()), val["name"].get<QString>(), this};
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error while parsing project: " << e.what() << std::endl;
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
            j["end_time"] = endTime.time().toString(QStringLiteral("HH:mm:ss"));
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
        std::cerr << "Unhandled time entry action: " << __FILE__ << ":" << __LINE__;
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
