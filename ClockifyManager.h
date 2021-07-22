#ifndef CLOCKIFYMANAGER_H
#define CLOCKIFYMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSet>

#include <functional>
#include <iostream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ClockifyManager : public QObject
{
    Q_OBJECT

	Q_PROPERTY(bool isValid READ isValid CONSTANT)

public:
	explicit ClockifyManager(QByteArray workspaceId, QByteArray apiKey, QObject *parent = nullptr);

	bool isValid() const { return m_isValid; }

	QList<QPair<QString, QString>> projects();
	QList<QPair<QString, QString>> users();

	QString projectName(const QString &projectId) const;
	QString userName(const QString &userId) const;

	bool userHasRunningTimeEntry(const QString &userId);
	void stopRunningTimeEntry(const QString &userId, bool async);
	json getRunningTimeEntry(const QString &userId);
	void startTimeEntry(const QString &userId, const QString &projectId, bool async);
	void startTimeEntry(const QString &userId, const QString &projectId, const QString &description, bool async);

signals:
	void projectsLoaded();
	void usersLoaded();

private:
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

	QByteArray m_workspaceId;
	const QByteArray m_apiKey;

	// these are ordered as <id, name>
	QList<QPair<QString, QString>> m_projects;
	QList<QPair<QString, QString>> m_users;

	QNetworkAccessManager m_manager{this};

	// When get() is called, insert the reply with its 2 callbacks. This is necessary because if we just try to capture the
	// callbacks in the lambda that is connected to QNetworkReply::finished in get(), the callbacks will go out of scope and
	// cause a segfault. Therefore, we need to stash them, and I figured that stashing them in a QHash was a lot better than
	// trying to juggle pointers to dynamically-allocated std::functions.
	QHash<QNetworkReply *, QPair<std::function<void (QNetworkReply *)>, std::function<void (QNetworkReply *)>>> m_pendingReplies;

	const static QByteArray s_baseUrl;

	bool m_isValid{false};
	bool m_projectsLoaded{false};
	bool m_usersLoaded{false};

	static const std::function<void (QNetworkReply *)> s_defaultSuccessCb;
	static const std::function<void (QNetworkReply *)> s_defaultFailureCb;
};

#endif // CLOCKIFYMANAGER_H