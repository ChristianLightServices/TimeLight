#ifndef ABSTRACTTIMESERVICEMANAGER_H
#define ABSTRACTTIMESERVICEMANAGER_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QSet>
#include <QTimer>

#include <spdlog/spdlog.h>

#include <functional>
#include <optional>

#include "JsonHelper.h"
#include "Project.h"
#include "Workspace.h"

class User;
class TimeEntry;

using NetworkReplyCallback = std::function<void(QNetworkReply *)>;

class AbstractTimeServiceManager : public QObject
{
    Q_OBJECT

public:
    explicit AbstractTimeServiceManager(const QByteArray &apiKey, QObject *parent = nullptr);

    enum Pagination
    {
        None = 0,
        Users = 1 << 0,
        Projects = 1 << 1,
        TimeEntries = 1 << 2,
        Workspaces = 1 << 3,
    };
    Q_DECLARE_FLAGS(SupportedPagination, Pagination)

    bool isValid() const { return m_isValid; }

    QVector<Project> &projects();
    QVector<Workspace> &workspaces();
    QVector<QPair<QString, QString>> &users();

    QString projectName(const QString &projectId);
    QString userName(const QString &userId);

    void startTimeEntry(const QString &userId, const Project &project, bool async);
    void startTimeEntry(const QString &userId, const Project &project, const QDateTime &start, bool async);
    std::optional<TimeEntry> getRunningTimeEntry(const QString &userId);
    QDateTime stopRunningTimeEntry(const QString &userId, bool async);
    void modifyTimeEntry(const QString &userId, const TimeEntry &t, bool async);
    //! The time entries returned by this function will always be sorted in descending order, i.e. the most recent entry will
    //! be first.
    QVector<TimeEntry> getTimeEntries(const QString &userId,
                                      std::optional<int> pageNumber,
                                      std::optional<int> pageSize,
                                      std::optional<QDateTime> start = std::nullopt,
                                      std::optional<QDateTime> end = std::nullopt);
    void deleteTimeEntry(const QString &userId, const TimeEntry &t, bool async);

    bool isConnectedToInternet() const { return m_isConnectedToInternet; }
    User getApiKeyOwner();
    Workspace currentWorkspace();

    QString apiKey() const { return m_apiKey; }
    QString workspaceId() const { return m_workspaceId; }

    void setApiKey(const QString &apiKey);
    void setWorkspaceId(const QString &workspaceId);

    //! Register a custom logger object to be used.

    //! By default, AbstractTimeServiceManager generates a basic spdlog::logger object. If you wish to register a custom
    //! spdlog::logger for use by AbstractTimeServiceManager, pass it to this function. To regenerate the default handler,
    //! call this function with `nullptr` as the parameter.
    void setLogger(std::shared_ptr<spdlog::logger> newLogger);

    // ***** BEGIN FUNCTIONS THAT SHOULD BE OVERRIDDEN *****

    //! Override this to provide a unique identifier for your time service. Generally, taking the form `com.clockify` should
    //! be fine. Once set, this identifier should not be changed as it may be used for storing settings specific to your time
    //! service.
    virtual QString serviceIdentifier() const = 0;
    //! This function provides the application with a service name appropriate for display to the user (e.g. "Clockify").
    virtual QString serviceName() const = 0;
    //! This returns a URL to the tracker webpage of the time service. If your time service has no such feature, return a URL
    //! to the webpage of the official, most fully-featured, or best-known app	or other software for managing your time
    //! service.
    virtual QUrl timeTrackerWebpageUrl() const = 0;

    //! The URL that is the base of all API requests.
    virtual const QString apiBaseUrl() const = 0;

    //! A list of items that may be loaded with pagination.
    virtual const QFlags<Pagination> supportedPagination() const = 0;

    //! This function should be overridden if your service starts pagination at a value other than 1.
    virtual int paginationStartsAt() const { return 1; }

    //! Return a function that will return the current date and time as defined by your time service. Generally, you will
    //! need to choose either UTC or the local time. For these, QDateTime::currentDateTime() and
    //! QDateTime::currentDateTimeUtc() should suffice.
    virtual const QDateTime currentDateTime() const = 0;

    // ***** END FUNCTIONS THAT SHOULD BE OVERRIDDEN *****

signals:
    void invalidated();
    //! Generally, services will start returning HTTP 429 if you make too many requests at once. This signal allows you to
    //! track whether you are being ratelimited in this manner.
    void ratelimited(bool status);
    void apiKeyChanged();

    void internetConnectionChanged(bool status);

protected:
    enum class HttpVerb
    {
        Get,
        Post,
        Patch,
        Put,
        Head,
        Delete,
    };
    enum class TimeEntryAction
    {
        StartTimeEntry,
        GetRunningTimeEntry,
        StopTimeEntry,
        ModifyTimeEntry,
        DeleteTimeEntry,
    };

    // ***** BEGIN FUNCTIONS THAT SHOULD BE OVERRIDDEN *****
    virtual const QByteArray authHeaderName() const = 0;

    //! This function will have the API key substituted into it with arg(). Use if your time service requires more text
    //! than just the API key for authorization.
    virtual const QString apiKeyTemplate() const { return QStringLiteral("%1"); }

    virtual QUrl runningTimeEntryUrl(const QString &userId, const QString &workspaceId) = 0;
    virtual QUrl startTimeEntryUrl(const QString &userId, const QString &workspaceId) = 0;
    virtual QUrl stopTimeEntryUrl(const QString &userId, const QString &workspaceId) = 0;
    virtual QUrl modifyTimeEntryUrl(const QString &userId, const QString &workspaceId, const QString &timeEntryId) = 0;
    virtual QUrl deleteTimeEntryUrl(const QString &userId, const QString &workspaceId, const QString &timeEntryId) = 0;
    virtual QUrl timeEntryUrl(const QString &userId, const QString &workspaceId, const QString &timeEntryId) = 0;
    virtual QUrl timeEntriesUrl(const QString &userId,
                                const QString &workspaceId,
                                std::optional<QDateTime> start = std::nullopt,
                                std::optional<QDateTime> end = std::nullopt) const = 0;
    virtual QUrl currentUserUrl() const = 0;
    virtual QUrl workspacesUrl() const = 0;
    virtual QUrl usersUrl(const QString &workspaceId) const = 0;
    virtual QUrl projectsUrl(const QString &workspaceId) const = 0;

    virtual const QString usersPageSizeHeaderName() const { return {}; }
    virtual const QString projectsPageSizeHeaderName() const { return {}; }
    virtual const QString timeEntriesPageSizeHeaderName() const { return {}; }
    virtual const QString workspacesPageSizeHeaderName() const { return {}; }

    virtual const QString usersPageHeaderName() const { return {}; }
    virtual const QString projectsPageHeaderName() const { return {}; }
    virtual const QString timeEntriesPageHeaderName() const { return {}; }
    virtual const QString workspacesPageHeaderName() const { return {}; }

    virtual int usersPaginationPageSize() const { return 50; }
    virtual int projectsPaginationPageSize() const { return 50; }
    virtual int timeEntriesPaginationPageSize() const { return 50; }
    virtual int workspacesPaginationPageSize() const { return 50; }

    virtual std::optional<TimeEntry> jsonToRunningTimeEntry(const json &j) = 0;
    virtual TimeEntry jsonToTimeEntry(const json &j) = 0;
    virtual User jsonToUser(const json &j) = 0;
    virtual QPair<QString, QString> jsonToUserData(const json &j) = 0;
    virtual Workspace jsonToWorkspace(const json &j) = 0;
    virtual Project jsonToProject(const json &j) = 0;

    virtual json timeEntryToJson(const TimeEntry &t, TimeEntryAction action) = 0;

    //! Return a string representing the time format used by your time service. See
    //! https://doc.qt.io/qt-5/qtime.html#toString and https://doc.qt.io/qt-5/qdate.html#toString-1 for examples of how to
    //! build format strings.
    virtual const QString jsonTimeFormatString() const = 0;

    virtual HttpVerb httpVerbForAction(const TimeEntryAction action) const = 0;
    virtual int httpReturnCodeForVerb(const HttpVerb verb) const = 0;
    virtual QByteArray getRunningTimeEntryPayload() const { return {}; }
    //! Different services return time entries in different orders. If your time entries are returned with the most
    //! recent entry first, return Qt::DescendingOrder; otherwise, return Qt::AscendingOrder.
    virtual Qt::SortOrder timeEntriesSortOrder() const = 0;

    // Note: these names are reserved for future development purposes
    // virtual json userToJson(const User &u) = 0;
    // virtual json projectToJson(const Project &p) = 0;

    // ***** END FUNCTIONS THAT SHOULD BE OVERRIDDEN *****

    //! Call this function in the constructor of any derived, non-abstract class.
    void callInitVirtualMethods();

    //! Returns a general-purpose logger object.
    std::shared_ptr<spdlog::logger> logger() const { return m_logger; }

    //! Parse JSON into a QDateTime.
    QDateTime jsonToDateTime(const json &j) const;
    json dateTimeToJson(const QDateTime &dt) const;

private:
    void updateCurrentUser();
    void updateProjects();
    void updateWorkspaces();
    void updateUsers();

    QByteArray apiKeyForRequests() const { return apiKeyTemplate().arg(m_apiKey).toUtf8(); }

    void defaultSuccessCb(QNetworkReply *rep);
    void defaultFailureCb(QNetworkReply *reply);

    void timeEntryReq(const QUrl &url,
                      const TimeEntryAction action,
                      const QByteArray &body = {},
                      bool async = true,
                      NetworkReplyCallback successCb = {},
                      NetworkReplyCallback failureCb = {});

    void get(const QUrl &url, NetworkReplyCallback successCb = {}, NetworkReplyCallback failureCb = {});
    void get(const QUrl &url,
             int expectedReturnCode,
             NetworkReplyCallback successCb = {},
             NetworkReplyCallback failureCb = {});
    void get(const QUrl &url, bool async, NetworkReplyCallback successCb = {}, NetworkReplyCallback failureCb = {});
    void get(const QUrl &url,
             bool async,
             int expectedReturnCode,
             NetworkReplyCallback successCb = {},
             NetworkReplyCallback failureCb = {});

    void post(const QUrl &url,
              const QByteArray &body,
              NetworkReplyCallback successCb = {},
              NetworkReplyCallback failureCb = {});
    void post(const QUrl &url,
              const QByteArray &body,
              int expectedReturnCode,
              NetworkReplyCallback successCb = {},
              NetworkReplyCallback failureCb = {});
    void post(const QUrl &url,
              const QByteArray &body,
              bool async,
              NetworkReplyCallback successCb = {},
              NetworkReplyCallback failureCb = {});
    void post(const QUrl &url,
              const QByteArray &body,
              bool async,
              int expectedReturnCode,
              NetworkReplyCallback successCb = {},
              NetworkReplyCallback failureCb = {});

    void patch(const QUrl &url,
               const QByteArray &body,
               NetworkReplyCallback successCb = {},
               NetworkReplyCallback failureCb = {});
    void patch(const QUrl &url,
               const QByteArray &body,
               int expectedReturnCode,
               NetworkReplyCallback successCb = {},
               NetworkReplyCallback failureCb = {});
    void patch(const QUrl &url,
               const QByteArray &body,
               bool async,
               NetworkReplyCallback successCb = {},
               NetworkReplyCallback failureCb = {});
    void patch(const QUrl &url,
               const QByteArray &body,
               bool async,
               int expectedReturnCode,
               NetworkReplyCallback successCb = {},
               NetworkReplyCallback failureCb = {});

    void put(const QUrl &url,
             const QByteArray &body,
             NetworkReplyCallback successCb = {},
             NetworkReplyCallback failureCb = {});
    void put(const QUrl &url,
             const QByteArray &body,
             int expectedReturnCode,
             NetworkReplyCallback successCb = {},
             NetworkReplyCallback failureCb = {});
    void put(const QUrl &url,
             const QByteArray &body,
             bool async,
             NetworkReplyCallback successCb = {},
             NetworkReplyCallback failureCb = {});
    void put(const QUrl &url,
             const QByteArray &body,
             bool async,
             int expectedReturnCode,
             NetworkReplyCallback successCb = {},
             NetworkReplyCallback failureCb = {});

    void head(const QUrl &url, NetworkReplyCallback successCb = {}, NetworkReplyCallback failureCb = {});
    void head(const QUrl &url,
              int expectedReturnCode,
              NetworkReplyCallback successCb = {},
              NetworkReplyCallback failureCb = {});
    void head(const QUrl &url, bool async, NetworkReplyCallback successCb = {}, NetworkReplyCallback failureCb = {});
    void head(const QUrl &url,
              bool async,
              int expectedReturnCode,
              NetworkReplyCallback successCb = {},
              NetworkReplyCallback failureCb = {});

    void del(const QUrl &url,
             const QByteArray &body,
             NetworkReplyCallback successCb = {},
             NetworkReplyCallback failureCb = {});
    void del(const QUrl &url,
             const QByteArray &body,
             int expectedReturnCode,
             NetworkReplyCallback successCb = {},
             NetworkReplyCallback failureCb = {});
    void del(const QUrl &url,
             const QByteArray &body,
             bool async,
             NetworkReplyCallback successCb = {},
             NetworkReplyCallback failureCb = {});
    void del(const QUrl &url,
             const QByteArray &body,
             bool async,
             int expectedReturnCode,
             NetworkReplyCallback successCb = {},
             NetworkReplyCallback failureCb = {});

    std::shared_ptr<spdlog::logger> m_logger;

    QString m_workspaceId;
    QByteArray m_apiKey;

    QVector<Project> m_projects;
    QVector<Workspace> m_workspaces;
    // this is ordered as <id, name>
    QVector<QPair<QString, QString>> m_users;

    QNetworkAccessManager m_manager{this};

    // When get() is called, insert the reply with its 2 callbacks. This is necessary because if we just try to capture the
    // callbacks in the lambda that is connected to QNetworkReply::finished in get(), the callbacks will go out of scope and
    // cause a segfault. Therefore, we need to stash them, and I figured that stashing them in a QHash was a lot better than
    // trying to juggle pointers to dynamically-allocated std::functions.
    QHash<QNetworkReply *, QPair<NetworkReplyCallback, NetworkReplyCallback>> m_pendingReplies;

    QTimer m_expireProjectsTimer;
    QTimer m_expireWorkspacesTimer;
    QTimer m_expireUsersTimer;

    bool m_isValid{false};
    bool m_projectsLoaded{false};
    bool m_workspacesLoaded{false};
    bool m_usersLoaded{false};
    bool m_loadingProjects{false};
    bool m_loadingWorkspaces{false};
    bool m_loadingUsers{false};
    bool m_isConnectedToInternet{true}; // assume connected at start
    bool m_isRatelimited{false};
};
Q_DECLARE_OPERATORS_FOR_FLAGS(AbstractTimeServiceManager::SupportedPagination)

template<class Manager>
concept TimeManager = std::is_base_of<AbstractTimeServiceManager, Manager>::value;

#endif // ABSTRACTTIMESERVICEMANAGER_H
