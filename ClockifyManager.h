#ifndef CLOCKIFYMANAGER_H
#define CLOCKIFYMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSet>
#include <QTimer>

#include <functional>
#include <iostream>

#include <nlohmann/json.hpp>

#include "ClockifyProject.h"

using json = nlohmann::json;

class ClockifyUser;
class TimeEntry;

class ClockifyManager : public QObject
{
    Q_OBJECT

	Q_PROPERTY(bool isValid READ isValid CONSTANT)

public:
	explicit ClockifyManager(QString workspaceId, QByteArray apiKey, QObject *parent = nullptr);

	bool isValid() const { return m_isValid; }

	QVector<ClockifyProject> &projects();
	QVector<QPair<QString, QString>> &users();

	QString projectName(const QString &projectId);
	QString userName(const QString &userId);

	bool userHasRunningTimeEntry(const QString &userId);
	QDateTime stopRunningTimeEntry(const QString &userId, bool async);
	TimeEntry getRunningTimeEntry(const QString &userId);
	void startTimeEntry(const QString &userId, const QString &projectId, bool async);
	void startTimeEntry(const QString &userId, const QString &projectId, const QString &description, bool async);
	void startTimeEntry(const QString &userId, const QString &projectId, const QDateTime &start, bool async);
	void startTimeEntry(const QString &userId, const QString &projectId, const QString &description, const QDateTime &start, bool async);
	QVector<TimeEntry> getTimeEntries(const QString &userId);

	bool isConnectedToInternet() const { return m_isConnectedToInternet; }
	ClockifyUser *getApiKeyOwner();

	QString apiKey() const { return m_apiKey; }
	void setApiKey(const QString &apiKey);

	static bool init(QString workspaceId, QByteArray apiKey);
	static QSharedPointer<ClockifyManager> instance() { return s_instance; }

signals:
	void projectsLoaded();
	void usersLoaded();

	void invalidated();
	void apiKeyChanged();

	void internetConnectionChanged(bool status);

private:
	void updateCurrentUser();
	void updateUsers();
	void updateProjects();

	QNetworkReply *get(const QUrl &url,
					   std::function<void (QNetworkReply *)> successCb = s_defaultSuccessCb,
					   std::function<void (QNetworkReply *)> failureCb = s_defaultFailureCb);
	QNetworkReply *get(const QUrl &url,
					   int expectedReturnCode,
					   std::function<void (QNetworkReply *)> successCb = s_defaultSuccessCb,
					   std::function<void (QNetworkReply *)> failureCb = s_defaultFailureCb);

	QNetworkReply *post(const QUrl &url,
						const QByteArray &body,
						std::function<void (QNetworkReply *)> successCb = s_defaultSuccessCb,
						std::function<void (QNetworkReply *)> failureCb = s_defaultFailureCb);
	QNetworkReply *post(const QUrl &url,
						const QByteArray &body,
						int expectedReturnCode,
						std::function<void (QNetworkReply *)> successCb = s_defaultSuccessCb,
						std::function<void (QNetworkReply *)> failureCb = s_defaultFailureCb);

	QNetworkReply *patch(const QUrl &url,
						 const QByteArray &body,
						 std::function<void (QNetworkReply *)> successCb = s_defaultSuccessCb,
						 std::function<void (QNetworkReply *)> failureCb = s_defaultFailureCb);
	QNetworkReply *patch(const QUrl &url,
						 const QByteArray &body,
						 int expectedReturnCode,
						 std::function<void (QNetworkReply *)> successCb = s_defaultSuccessCb,
						 std::function<void (QNetworkReply *)> failureCb = s_defaultFailureCb);

	QNetworkReply *head(const QUrl &url,
						 std::function<void (QNetworkReply *)> successCb = s_defaultSuccessCb,
						 std::function<void (QNetworkReply *)> failureCb = s_defaultFailureCb);
	QNetworkReply *head(const QUrl &url,
						 int expectedReturnCode,
						 std::function<void (QNetworkReply *)> successCb = s_defaultSuccessCb,
						 std::function<void (QNetworkReply *)> failureCb = s_defaultFailureCb);

	QString m_workspaceId;
	QByteArray m_apiKey;

	QString m_ownerId;

	QVector<ClockifyProject> m_projects;

	// this is ordered as <id, name>
	QVector<QPair<QString, QString>> m_users;

	QNetworkAccessManager m_manager{this};

	// When get() is called, insert the reply with its 2 callbacks. This is necessary because if we just try to capture the
	// callbacks in the lambda that is connected to QNetworkReply::finished in get(), the callbacks will go out of scope and
	// cause a segfault. Therefore, we need to stash them, and I figured that stashing them in a QHash was a lot better than
	// trying to juggle pointers to dynamically-allocated std::functions.
	QHash<QNetworkReply *, QPair<std::function<void (QNetworkReply *)>, std::function<void (QNetworkReply *)>>> m_pendingReplies;

	const static QString s_baseUrl;

	bool m_isValid{false};
	bool m_projectsLoaded{false};
	bool m_usersLoaded{false};
	bool m_projectsStale{true};
	bool m_usersStale{true};
	bool m_isConnectedToInternet{true}; // assume connected at start

	QTimer m_markUsersAsStaleTimer;
	QTimer m_markProjectsAsStaleTimer;
	QTimer m_checkConnectionTimer;

	static const std::function<void (QNetworkReply *)> s_defaultSuccessCb;
	static const std::function<void (QNetworkReply *)> s_defaultFailureCb;
	static QSharedPointer<ClockifyManager> s_instance;
};

#endif // CLOCKIFYMANAGER_H
