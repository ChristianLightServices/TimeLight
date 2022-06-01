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
    virtual const QFlags<Pagination> supportedPagination() const final;
    virtual Qt::SortOrder timeEntriesSortOrder() const final { return Qt::DescendingOrder; }

protected:
    virtual const QByteArray authHeaderName() const final { return QByteArrayLiteral("X-Api-Key"); }

    virtual const QString baseUrl() const final { return QStringLiteral("https://api.clockify.me/api/v1"); }
    virtual QUrl runningTimeEntryUrl(const QString &userId, const QString &workspaceId) final;
    virtual QUrl startTimeEntryUrl(const QString &userId, const QString &workspaceId) final;
    virtual QUrl stopTimeEntryUrl(const QString &userId, const QString &workspaceId) final;
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

    virtual bool jsonToHasRunningTimeEntry(const json &j) final;
    virtual std::optional<TimeEntry> jsonToRunningTimeEntry(const json &j) final;
    virtual TimeEntry jsonToTimeEntry(const json &j) final;
    virtual User jsonToUser(const json &j) final;
    virtual QPair<QString, QString> jsonToUserData(const json &j) final;
    virtual Workspace jsonToWorkspace(const json &j) final;
    virtual Project jsonToProject(const json &j) final;

    virtual const QString jsonTimeFormatString() const final { return QStringLiteral("yyyy-MM-ddTHH:mm:ssZ"); }
    virtual const QDateTime currentDateTime() const final { return QDateTime::currentDateTimeUtc(); }

    virtual json timeEntryToJson(const TimeEntry &t, TimeEntryAction) final;

    virtual HttpVerb httpVerbForAction(const TimeEntryAction action) const final;
    virtual int httpReturnCodeForVerb(const HttpVerb verb) const final;
};

#endif // CLOCKIFYMANAGER_H
