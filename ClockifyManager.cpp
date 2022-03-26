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

QUrl ClockifyManager::timeEntryUrl([[maybe_unused]] const QString &userId, const QString &workspaceId, const QString &timeEntryId)
{
    return {baseUrl() + "/workspaces/" + workspaceId + "/time-entries/" + timeEntryId};
}

QUrl ClockifyManager::timeEntriesUrl(const QString &userId, const QString &workspaceId) const
{
    return {baseUrl() + "/workspaces/" + workspaceId + "/user/" + userId + "/time-entries"};
}

QUrl ClockifyManager::currentUserUrl() const
{
    return {baseUrl() + "/user"};
}

QUrl ClockifyManager::workspacesUrl() const
{
    return {baseUrl() + "/workspaces"};
}

QUrl ClockifyManager::usersUrl(const QString &workspaceId) const
{
    return {baseUrl() + "/workspaces/" + workspaceId + "/users"};
}

QUrl ClockifyManager::projectsUrl(const QString &workspaceId) const
{
    return {baseUrl() + "/workspaces/" + workspaceId + "/projects"};
}

const QFlags<AbstractTimeServiceManager::Pagination> ClockifyManager::supportedPagination() const
{
    QFlags<Pagination> f;
    f.setFlag(Pagination::Projects);
    f.setFlag(Pagination::Users);
    f.setFlag(Pagination::TimeEntries);
    return f;
}

bool ClockifyManager::jsonToHasRunningTimeEntry(const nlohmann::json &j)
{
    return !j.empty();
}

TimeEntry ClockifyManager::jsonToRunningTimeEntry(const nlohmann::json &j)
{
    return jsonToTimeEntry(j);
}

TimeEntry ClockifyManager::jsonToTimeEntry(const nlohmann::json &j)
{
    try
    {
        if (j.empty())
            return {this};

        auto entry = (j.is_array() ? j[0] : j);

        auto id = entry["id"].get<QString>();
        QString projectId;
        Project project;
        if (entry.contains("projectId") && !entry["projectId"].is_null())
        {
            projectId = entry["projectId"].get<QString>();
            project = {projectId, projectName(projectId), entry.contains("description") ? entry["description"].get<QString>() : QString{}};
        }
        auto userId = entry["userId"].get<QString>();
        auto start = entry["timeInterval"]["start"].get<QDateTime>();
        QDateTime end;
        if (entry["timeInterval"].contains("end") && !entry["timeInterval"]["end"].is_null())
            end = entry["timeInterval"]["end"].get<QDateTime>();

        return TimeEntry{id, project, project.description(), userId, start, end, this};
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error while creating time entry: " << e.what() << std::endl;
        return {this};
    }
}

User ClockifyManager::jsonToUser(const nlohmann::json &j)
{
    try
    {
        return User{j["id"].get<QString>(), j["name"].get<QString>(), j["defaultWorkspace"].get<QString>(), this};
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error while parsing user: " << e.what() << std::endl;
        return {this};
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
        std::cerr << "Error while parsing users: " << e.what() << std::endl;
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
        std::cerr << "Error while parsing workspace: " << e.what() << std::endl;
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
        std::cerr << "Error while parsing project: " << e.what() << std::endl;
        return {this};
    }
}

json ClockifyManager::timeEntryToJson(const TimeEntry &t)
{
    json j;
    j["start"] = t.start();
    if (!t.end().isNull())
        j["end"] = t.end();
    if (!t.description().isEmpty())
        j["description"] = t.description();
    j["projectId"] = t.project().id();
    return j;
}
