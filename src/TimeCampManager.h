#ifndef TIMECAMPMANAGER_H
#define TIMECAMPMANAGER_H

#include "AbstractTimeServiceManager.h"
#include "src/TimeEntry.h"

class TimeCampManager : public AbstractTimeServiceManager
{
    Q_OBJECT

public:
    explicit TimeCampManager(const QByteArray &apiKey, QObject *parent = nullptr);

    virtual QString serviceIdentifier() const final { return QStringLiteral("com.timecamp"); }
    virtual QString serviceName() const final { return QStringLiteral("TimeCamp"); }
    virtual QUrl timeTrackerWebpageUrl() const final { return QUrl{QStringLiteral("https://app.timecamp.com/dashboard")}; }
    virtual const QString apiBaseUrl() const final { return QStringLiteral("https://app.timecamp.com/third_party/api"); }
    virtual const QFlags<Pagination> supportedPagination() const final;

protected:
    virtual const QByteArray authHeaderName() const final { return QByteArrayLiteral("Authorization"); }

    virtual QUrl runningTimeEntryUrl(const QString &userId, const QString &workspaceId) final;
    virtual QUrl startTimeEntryUrl(const QString &userId, const QString &workspaceId) final;
    virtual QUrl stopTimeEntryUrl(const QString &userId, const QString &workspaceId) final;
    virtual QUrl modifyTimeEntryUrl(const QString &userId, const QString &workspaceId, const QString &timeEntryId) final;
    virtual QUrl deleteTimeEntryUrl(const QString &userId, const QString &workspaceId, const QString &timeEntryId) final;
    virtual QUrl timeEntryUrl(const QString &userId, const QString &workspaceId, const QString &timeEntryId) final;
    virtual QUrl timeEntriesUrl(const QString &userId,
                                const QString &workspaceId,
                                std::optional<QDateTime> start = std::nullopt,
                                std::optional<QDateTime> end = std::nullopt) const final;
    virtual QUrl currentUserUrl() const final;
    virtual QUrl workspacesUrl() const final;
    virtual QUrl usersUrl(const QString &workspaceId) const final;
    virtual QUrl projectsUrl(const QString &workspaceId) const final;

    virtual std::optional<TimeEntry> jsonToRunningTimeEntry(const json &j) final;
    virtual TimeEntry jsonToTimeEntry(const json &j) final;
    virtual QSharedPointer<User> jsonToUser(const json &j) final;
    virtual QPair<QString, QString> jsonToUserData(const json &j) final;
    virtual Workspace jsonToWorkspace(const json &j) final;
    virtual Project jsonToProject(const json &j) final;

    virtual const QDateTime stringToDateTime(const QString &string) const final
    {
        return QDateTime::fromString(string, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
    virtual const QString dateTimeToString(const QDateTime &date) const final
    {
        return date.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }

    virtual json timeEntryToJson(const TimeEntry &t, TimeEntryAction action) final;

    virtual HttpVerb httpVerbForAction(const TimeEntryAction action) const final;
    virtual int httpReturnCodeForVerb(const HttpVerb verb) const final;
    virtual QByteArray getRunningTimeEntryPayload() const final { return QByteArrayLiteral(R"({"action":"status"})"); }
    virtual Qt::SortOrder timeEntriesSortOrder() const final { return Qt::AscendingOrder; }
};

#endif // TIMECAMPMANAGER_H
