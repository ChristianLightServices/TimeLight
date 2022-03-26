#ifndef ABSTRACTTIMESERVICEMANAGER_H
#define ABSTRACTTIMESERVICEMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSet>
#include <QTimer>

#include <functional>
#include <iostream>
#include <optional>

#include "JsonHelper.h"
#include "Project.h"
#include "Workspace.h"

class User;
class TimeEntry;

using NetworkReplyCallback = std::function<void (QNetworkReply *)>;

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
	QVector<QPair<QString, QString>> &users();

	QString projectName(const QString &projectId);
	QString userName(const QString &userId);

	virtual bool userHasRunningTimeEntry(const QString &userId);
	QDateTime stopRunningTimeEntry(const QString &userId, bool async);
	TimeEntry getRunningTimeEntry(const QString &userId);
	void startTimeEntry(const QString &userId, const QString &projectId, bool async);
	void startTimeEntry(const QString &userId, const QString &projectId, const QString &description, bool async);
	void startTimeEntry(const QString &userId, const QString &projectId, const QDateTime &start, bool async);
	void startTimeEntry(const QString &userId, const QString &projectId, const QString &description, const QDateTime &start, bool async);
	QVector<TimeEntry> getTimeEntries(const QString &userId, std::optional<int> pageNumber, std::optional<int> pageSize);

	bool isConnectedToInternet() const { return m_isConnectedToInternet; }
	User getApiKeyOwner();
	QVector<Workspace> getOwnerWorkspaces();

	QString apiKey() const { return m_apiKey; }
	QString workspaceId() const { return m_workspaceId; }

	void setApiKey(const QString &apiKey);
	void setWorkspaceId(const QString &workspaceId);

	//! This function should be overridden if your service starts pagination at a value other than 1.
	virtual int paginationStartsAt() const { return 1; }

signals:
	void invalidated();
	void apiKeyChanged();

	void internetConnectionChanged(bool status);

protected:
	// ***** BEGIN FUNCTIONS THAT SHOULD BE OVERRIDDEN *****
	virtual const QByteArray authHeaderName() const = 0;

	virtual const QString baseUrl() const = 0;
	virtual QUrl runningTimeEntryUrl(const QString &userId, const QString &workspaceId) = 0;
	virtual QUrl timeEntryUrl(const QString &userId, const QString &workspaceId, const QString &timeEntryId) = 0;
	virtual QUrl timeEntriesUrl(const QString &userId, const QString &workspaceId) const = 0;
	virtual QUrl currentUserUrl() const = 0;
	virtual QUrl workspacesUrl() const = 0;
	virtual QUrl usersUrl(const QString &workspaceId) const = 0;
	virtual QUrl projectsUrl(const QString &workspaceId) const = 0;

	virtual const QFlags<Pagination> supportedPagination() const = 0;

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

	virtual TimeEntry jsonToRunningTimeEntry(const json &j) = 0;
	virtual TimeEntry jsonToTimeEntry(const json &j) = 0;
	virtual User jsonToUser(const json &j) = 0;
	virtual QPair<QString, QString> jsonToUserData(const json &j) = 0;
	virtual Workspace jsonToWorkspace(const json &j) = 0;
	virtual Project jsonToProject(const json &j) = 0;

	virtual json timeEntryToJson(const TimeEntry &t) = 0;

	// Note: these names are reserved for future development purposes
	// virtual json userToJson(const User &u) = 0;
	// virtual json projectToJson(const Project &p) = 0;

	// ***** END FUNCTIONS THAT SHOULD BE OVERRIDDEN *****

	//! Call this function in the constructor of any derived, non-abstract class.
	void callInitVirtualMethods();

private:
	void updateCurrentUser();
	void updateUsers();
	void updateProjects();

	void get(const QUrl &url,
	                   const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                   const NetworkReplyCallback &failureCb = s_defaultFailureCb);
	void get(const QUrl &url,
	                   int expectedReturnCode,
	                   const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                   const NetworkReplyCallback &failureCb = s_defaultFailureCb);
	void get(const QUrl &url,
	                   bool async,
	                   const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                   const NetworkReplyCallback &failureCb = s_defaultFailureCb);
	void get(const QUrl &url,
	                   bool async,
	                   int expectedReturnCode,
	                   const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                   const NetworkReplyCallback &failureCb = s_defaultFailureCb);

	void post(const QUrl &url,
	                    const QByteArray &body,
	                    const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                    const NetworkReplyCallback &failureCb = s_defaultFailureCb);
	void post(const QUrl &url,
	                    const QByteArray &body,
	                    int expectedReturnCode,
	                    const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                    const NetworkReplyCallback &failureCb = s_defaultFailureCb);
	void post(const QUrl &url,
	                    const QByteArray &body,
	                    bool async,
	                    const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                    const NetworkReplyCallback &failureCb = s_defaultFailureCb);
	void post(const QUrl &url,
	                    const QByteArray &body,
	                    bool async,
	                    int expectedReturnCode,
	                    const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                    const NetworkReplyCallback &failureCb = s_defaultFailureCb);

	void patch(const QUrl &url,
	                     const QByteArray &body,
	                     const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                     const NetworkReplyCallback &failureCb = s_defaultFailureCb);
	void patch(const QUrl &url,
	                     const QByteArray &body,
	                     int expectedReturnCode,
	                     const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                     const NetworkReplyCallback &failureCb = s_defaultFailureCb);
	void patch(const QUrl &url,
	                     const QByteArray &body,
	                     bool async,
	                     const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                     const NetworkReplyCallback &failureCb = s_defaultFailureCb);
	void patch(const QUrl &url,
	                     const QByteArray &body,
	                     bool async,
	                     int expectedReturnCode,
	                     const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                     const NetworkReplyCallback &failureCb = s_defaultFailureCb);

	void head(const QUrl &url,
	                    const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                    const NetworkReplyCallback &failureCb = s_defaultFailureCb);
	void head(const QUrl &url,
	                    int expectedReturnCode,
	                    const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                    const NetworkReplyCallback &failureCb = s_defaultFailureCb);
	void head(const QUrl &url,
	                    bool async,
	                    const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                    const NetworkReplyCallback &failureCb = s_defaultFailureCb);
	void head(const QUrl &url,
	                    bool async,
	                    int expectedReturnCode,
	                    const NetworkReplyCallback &successCb = s_defaultSuccessCb,
	                    const NetworkReplyCallback &failureCb = s_defaultFailureCb);

	QString m_workspaceId;
	QByteArray m_apiKey;

	QString m_ownerId;

	QVector<Project> m_projects;

	// this is ordered as <id, name>
	QVector<QPair<QString, QString>> m_users;
	QHash<QString, int> m_numUsers;

	QNetworkAccessManager m_manager{this};

	// When get() is called, insert the reply with its 2 callbacks. This is necessary because if we just try to capture the
	// callbacks in the lambda that is connected to QNetworkReply::finished in get(), the callbacks will go out of scope and
	// cause a segfault. Therefore, we need to stash them, and I figured that stashing them in a QHash was a lot better than
	// trying to juggle pointers to dynamically-allocated std::functions.
	QHash<QNetworkReply *, QPair<NetworkReplyCallback, NetworkReplyCallback>> m_pendingReplies;

	QTimer m_expireUsersTimer;
	QTimer m_expireProjectsTimer;
	QTimer m_checkConnectionTimer;

	static const NetworkReplyCallback s_defaultSuccessCb;
	static const NetworkReplyCallback s_defaultFailureCb;

	bool m_isValid{false};
	bool m_projectsLoaded{false};
	bool m_usersLoaded{false};
	bool m_loadingProjects{false};
	bool m_loadingUsers{false};
	bool m_isConnectedToInternet{true}; // assume connected at start
};
Q_DECLARE_OPERATORS_FOR_FLAGS(AbstractTimeServiceManager::SupportedPagination)

#endif // ABSTRACTTIMESERVICEMANAGER_H
