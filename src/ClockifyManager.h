#ifndef CLOCKIFYMANAGER_H
#define CLOCKIFYMANAGER_H

#include "AbstractTimeServiceManager.h"

class ClockifyManager : public AbstractTimeServiceManager
{
    Q_OBJECT

public:
    explicit ClockifyManager(const QByteArray &apiKey, QObject *parent = nullptr);

    virtual QString serviceIdentifier() const final { return QStringLiteral("com.clockify"); }
    virtual QString serviceName() const final { return QStringLiteral("Clockify"); }
    virtual QUrl timeTrackerWebpageUrl() const final { return QUrl{QStringLiteral("https://clockify.me/tracker")}; }
    virtual const QString apiBaseUrl() const final { return QStringLiteral("https://api.clockify.me/api/v1"); }
    virtual const QFlags<Pagination> supportedPagination() const final;

protected:
    virtual const QByteArray authHeaderName() const final { return QByteArrayLiteral("X-Api-Key"); }

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

    virtual const QString projectsPageSizeHeaderName() const final { return QStringLiteral("page-size"); }
    virtual const QString usersPageSizeHeaderName() const final { return QStringLiteral("page-size"); }
    virtual const QString timeEntriesPageSizeHeaderName() const final { return QStringLiteral("page-size"); }

    virtual const QString projectsPageHeaderName() const final { return QStringLiteral("page"); }
    virtual const QString usersPageHeaderName() const final { return QStringLiteral("page"); }
    virtual const QString timeEntriesPageHeaderName() const final { return QStringLiteral("page"); }

    virtual std::optional<TimeEntry> jsonToRunningTimeEntry(const json &j) final;
    virtual TimeEntry jsonToTimeEntry(const json &j) final;
    virtual QSharedPointer<User> jsonToUser(const json &j) final;
    virtual QPair<QString, QString> jsonToUserData(const json &j) final;
    virtual Workspace jsonToWorkspace(const json &j) final;
    virtual Project jsonToProject(const json &j) final;

    virtual const QDateTime stringToDateTime(const QString &string) const final
    {
        return QDateTime::fromString(string, Qt::ISODate);
    }
    virtual const QString dateTimeToString(const QDateTime &date) const final { return date.toString(Qt::ISODate); }

    virtual json timeEntryToJson(const TimeEntry &t, TimeEntryAction action) final;

    virtual HttpVerb httpVerbForAction(const TimeEntryAction action) const final;
    virtual int httpReturnCodeForVerb(const HttpVerb verb) const final;
    virtual Qt::SortOrder timeEntriesSortOrder() const final { return Qt::DescendingOrder; }
};

#endif // CLOCKIFYMANAGER_H
