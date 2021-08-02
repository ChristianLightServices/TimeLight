#include "ClockifyManager.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QEventLoop>

#include <iostream>
#include <nlohmann/json.hpp>

#include "JsonHelper.h"
#include "ClockifyUser.h"
#include "ClockifyProject.h"
#include "TimeEntry.h"

using nlohmann::json;

const QByteArray ClockifyManager::s_baseUrl{"https://api.clockify.me/api/v1"};
const std::function<void (QNetworkReply *)> ClockifyManager::s_defaultSuccessCb = [](QNetworkReply *){};
const std::function<void (QNetworkReply *)> ClockifyManager::s_defaultFailureCb = [](QNetworkReply *reply) {

	if (auto code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); code == 0)
	{
		std::cerr << "Internet connection lost" << std::endl;
		ClockifyManager::instance()->m_isConnectedToInternet = false;
		emit ClockifyManager::instance()->internetConnectionChanged(false);
	}
	else
	{
		std::cerr << "Request " << reply->url().toString().toStdString()
				  << " failed with code "
				  << code
				  << std::endl;
	}
};
QSharedPointer<ClockifyManager> ClockifyManager::s_instance;

ClockifyManager::ClockifyManager(QByteArray workspaceId, QByteArray apiKey, QObject *parent)
	: QObject{parent},
	  m_workspaceId{workspaceId},
	  m_apiKey{apiKey}
{
	s_instance.reset(this);

	// request currently logged in user (the one whose API key we're using) as a validity test
	// and also in order to cache API key info
	QUrl url{s_baseUrl + "/user"};

	QNetworkRequest currentUser{url};
	currentUser.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	currentUser.setRawHeader("X-Api-Key", m_apiKey);

	auto userRep = m_manager.get(currentUser);
	QEventLoop loop;
	connect(userRep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	if (auto status = userRep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == 200)
	{
		try
		{
			json j{json::parse(userRep->readAll().toStdString())};
			m_ownerId = j["id"].get<QString>();
			m_isValid = true;
		}
		catch (const std::exception &ex)
		{
			std::cerr << "Error: " << ex.what() << std::endl;
			return;
		}
		catch (...)
		{
			std::cout << "Unknown error!\n";
			return;
		}
	}
	else if (status == 0)
	{
		std::cerr << "Internet connection not found" << std::endl;
		m_isConnectedToInternet = false;
	}
	else
	{
		std::cerr << "Request failed with code " << status << std::endl;
		return;
	}

	auto updateUsersAndProjects = [this]() {
		// get all projects for project list (note that this will return immediately and finish running in the
		// background; when the user calls projects(), projects() will block if needed until the projects are loaded)
		QUrl url{s_baseUrl + "/workspaces/" + m_workspaceId + "/projects"};

		// let's get _all_ the projects! *evil laughter*
		QUrlQuery query;
		query.addQueryItem("page-size", QString::number(INT_MAX));
		url.setQuery(query);

		get(url, [this](QNetworkReply *rep) {
			try
			{
				m_projectsLoaded = false;
				json j{json::parse(rep->readAll().toStdString())};

				m_projects.clear();
				for (const auto &item : j)
					m_projects.push_back({item["id"].get<QString>(),
										  item["name"].get<QString>()});

				m_projectsLoaded = true;
				emit projectsLoaded();
			}
			catch (...)
			{
				emit invalidated();
			}
		});

		// now load the users
		url = s_baseUrl + "/workspaces/" + m_workspaceId + "/users";
		query.clear();
		query.addQueryItem("page-size", QString::number(INT_MAX));
		url.setQuery(query);

		get(url, [this](QNetworkReply *rep) {
			try
			{
				m_usersLoaded = false;
				json j{json::parse(rep->readAll().toStdString())};

				m_users.clear();
				for (const auto &item : j)
					m_users.push_back({item["id"].get<QByteArray>(),
									   item["name"].get<QByteArray>()});

				m_usersLoaded = true;
				emit usersLoaded();
			}
			catch (...)
			{
				emit invalidated();
			}
		});
	};

	updateUsersAndProjects();

	m_updateCacheTimer.setInterval(60000); // every minute
	m_updateCacheTimer.callOnTimeout(updateUsersAndProjects);
	m_updateCacheTimer.setSingleShot(false);
	m_updateCacheTimer.start();

	m_checkConnectionTimer.setInterval(500);
	m_checkConnectionTimer.callOnTimeout([this]() {
		auto rep = m_manager.get(QNetworkRequest{QUrl{"https://clockify.me/"}});
		QEventLoop loop;
		connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
		loop.exec();

		bool connection = rep->bytesAvailable();
		if (m_isConnectedToInternet != connection)
		{
			m_isConnectedToInternet = connection;
			emit internetConnectionChanged(m_isConnectedToInternet);
		}

		rep->deleteLater();
	});
	m_checkConnectionTimer.setSingleShot(false);
	m_checkConnectionTimer.start();
}

QList<ClockifyProject> ClockifyManager::projects()
{
	if (!m_projectsLoaded)
	{
		QEventLoop loop;
		connect(this, &ClockifyManager::projectsLoaded, &loop, &QEventLoop::quit);
		loop.exec();
	}

	return m_projects;
}

QList<QPair<QString, QString>> ClockifyManager::users()
{
	if (!m_usersLoaded)
	{
		QEventLoop loop;
		connect(this, &ClockifyManager::usersLoaded, &loop, &QEventLoop::quit);
		loop.exec();
	}

	return m_users;
}

QString ClockifyManager::projectName(const QString &projectId) const
{
	if (!m_projectsLoaded)
	{
		QEventLoop loop;
		connect(this, &ClockifyManager::projectsLoaded, &loop, &QEventLoop::quit);
		loop.exec();
	}

	for (const auto &item : m_projects)
		if (item.id() == projectId)
			return item.name();

	return "";
}

QString ClockifyManager::userName(const QString &userId) const
{
	if (!m_usersLoaded)
	{
		QEventLoop loop;
		connect(this, &ClockifyManager::usersLoaded, &loop, &QEventLoop::quit);
		loop.exec();
	}

	for (const auto &item : m_users)
		if (item.first == userId)
			return item.second;

	return "";
}

bool ClockifyManager::userHasRunningTimeEntry(const QString &userId)
{
	QUrl url{s_baseUrl + "/workspaces/" + m_workspaceId + "/user/" + userId + "/time-entries"};
	QUrlQuery query;
	query.addQueryItem("in-progress", "true");
	url.setQuery(query);

	bool status = false;

	auto reply = get(url, [&status](QNetworkReply *rep) {
		try
		{
			json j{json::parse(rep->readAll().toStdString())};
			if (!j.empty())
				status = true;
		}
		catch (...)
		{
			// TODO: add some realistic error handling here
		}
	});

	QEventLoop loop;
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	return status;
}

QDateTime ClockifyManager::stopRunningTimeEntry(const QString &userId, bool async)
{
	auto now = QDateTime::currentDateTimeUtc();
	json j{{"end", now.toString("yyyy-MM-ddThh:mm:ssZ")}};

	QUrl url{s_baseUrl + "/workspaces/" + m_workspaceId + "/user/" + userId + "/time-entries"};
	auto rep = patch(url, QByteArray::fromStdString(j.dump()));

	if (!async)
	{
		QEventLoop loop;
		connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
		loop.exec();
	}

	return now;
}

TimeEntry ClockifyManager::getRunningTimeEntry(const QString &userId)
{
	QUrl url{s_baseUrl + "/workspaces/" + m_workspaceId + "/user/" + userId + "/time-entries"};
	QUrlQuery query;
	query.addQueryItem("in-progress", "true");
	url.setQuery(query);

	json j;

	auto reply = get(url, [&j](QNetworkReply *rep) {
		try
		{
			j = json::parse(rep->readAll().toStdString());
		}
		catch (...)
		{
			j = {};
		}
	});

	QEventLoop loop;
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	return TimeEntry{j[0]};
}

void ClockifyManager::startTimeEntry(const QString &userId, const QString &projectId, bool async)
{
	startTimeEntry(userId, projectId, QString{}, QDateTime::currentDateTimeUtc(), async);
}

void ClockifyManager::startTimeEntry(const QString &userId, const QString &projectId, const QString &description, bool async)
{
	startTimeEntry(userId, projectId, description, QDateTime::currentDateTimeUtc(), async);
}

void ClockifyManager::startTimeEntry(const QString &userId, const QString &projectId, const QDateTime &start, bool async)
{
	startTimeEntry(userId, projectId, QString{}, start, async);
}

void ClockifyManager::startTimeEntry(const QString &userId, const QString &projectId, const QString &description, const QDateTime &start, bool async)
{
	json j{
		{"start", start.toString("yyyy-MM-ddThh:mm:ssZ")},
		{"projectId", projectId}
	};

	if (!description.isEmpty())
		j += {"description", description};

	QUrl url{s_baseUrl + "/workspaces/" + m_workspaceId + "/user/" + userId + "/time-entries"};
	auto rep = post(url, QByteArray::fromStdString(j.dump()), 201);

	if (!async)
	{
		QEventLoop loop;
		connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
		loop.exec();
	}
}

QVector<TimeEntry> ClockifyManager::getTimeEntries(const QString &userId)
{
	json j;

	auto rep = get(QUrl{s_baseUrl + "/workspaces/" + m_workspaceId + "/user/" + userId + "/time-entries"}, [&j](QNetworkReply *rep) {
		try
		{
			j = json::parse(rep->readAll().toStdString());
		}
		catch (std::exception ex)
		{
			std::cout << ex.what() << std::endl;
			j = {};
		}
		catch (...)
		{
			std::cout << "Unknown error!\n";
			j = {};
		}
	});

	QEventLoop loop;
	connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	QVector<TimeEntry> entries;
	for (const auto &entry : j)
		entries.push_back(TimeEntry{entry});

	return entries;
}

ClockifyUser *ClockifyManager::getApiKeyOwner()
{
	// TODO: make sure to always use the correct ID for the API key
	if (!m_ownerId.isEmpty())
		return new ClockifyUser{m_ownerId, this};
	else
	{
		ClockifyUser *retVal{nullptr};

		auto rep = get(QUrl{s_baseUrl + "/user"}, [this, &retVal](QNetworkReply *rep) {
			try
			{
				json j{json::parse(rep->readAll().toStdString())};
				retVal = new ClockifyUser{j["id"].get<QString>(), this};
			}
			catch (std::exception ex)
			{
				std::cout << ex.what() << std::endl;
				retVal = nullptr;
			}
			catch (...)
			{
				std::cout << "Unknown error!\n";
				retVal = nullptr;
			}
		});

		QEventLoop loop;
		connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
		loop.exec();

		return retVal;
	}
}

void ClockifyManager::setApiKey(const QString &apiKey)
{
	m_apiKey = apiKey.toUtf8();
	emit apiKeyChanged();
}

// *****************************************************************************
// *****     BEYOND THIS POINT THERE ARE ONLY FUNCTIONS FOR REST STUFF     *****
// *****************************************************************************

QNetworkReply *ClockifyManager::get(const QUrl &url,
									  std::function<void (QNetworkReply *)> successCb,
									  std::function<void (QNetworkReply *)> failureCb)
{
	return get(url, 200, successCb, failureCb);
}

QNetworkReply *ClockifyManager::get(const QUrl &url,
									int expectedReturnCode,
									std::function<void (QNetworkReply *)> successCb,
									std::function<void (QNetworkReply *)> failureCb)
{
	QNetworkRequest req{url};
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setRawHeader("X-Api-Key", m_apiKey);

	auto rep = m_manager.get(req);
	m_pendingReplies.insert(rep, {successCb, failureCb});

	connect(rep, &QNetworkReply::finished, this, [this, rep, expectedReturnCode]() {
		if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == expectedReturnCode)
			m_pendingReplies[rep].first(rep);
		else
			m_pendingReplies[rep].second(rep);

		m_pendingReplies.remove(rep);
		rep->deleteLater();
	});

	return rep;
}

QNetworkReply *ClockifyManager::post(const QUrl &url,
									  const QByteArray &body,
									  std::function<void (QNetworkReply *)> successCb,
									  std::function<void (QNetworkReply *)> failureCb)
{
	return post(url, body, 200, successCb, failureCb);
}

QNetworkReply *ClockifyManager::post(const QUrl &url,
									 const QByteArray &body,
									 int expectedReturnCode,
									 std::function<void (QNetworkReply *)> successCb,
									 std::function<void (QNetworkReply *)> failureCb)
{
	QNetworkRequest req{url};
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setRawHeader("X-Api-Key", m_apiKey);

	auto rep = m_manager.post(req, body);
	m_pendingReplies.insert(rep, {successCb, failureCb});

	connect(rep, &QNetworkReply::finished, this, [this, rep, expectedReturnCode]() {
		if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == expectedReturnCode)
			m_pendingReplies[rep].first(rep);
		else
			m_pendingReplies[rep].second(rep);

		m_pendingReplies.remove(rep);
		rep->deleteLater();
	});

	return rep;
}

QNetworkReply *ClockifyManager::patch(const QUrl &url,
									  const QByteArray &body,
									  std::function<void (QNetworkReply *)> successCb,
									  std::function<void (QNetworkReply *)> failureCb)
{
	return patch(url, body, 200, successCb, failureCb);
}

QNetworkReply *ClockifyManager::patch(const QUrl &url,
									  const QByteArray &body,
									  int expectedReturnCode,
									  std::function<void (QNetworkReply *)> successCb,
									  std::function<void (QNetworkReply *)> failureCb)
{
	QNetworkRequest req{url};
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setRawHeader("X-Api-Key", m_apiKey);

	auto rep = m_manager.sendCustomRequest(req, "PATCH", body);
	m_pendingReplies.insert(rep, {successCb, failureCb});

	connect(rep, &QNetworkReply::finished, this, [this, rep, expectedReturnCode]() {
		if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == expectedReturnCode)
			m_pendingReplies[rep].first(rep);
		else
			m_pendingReplies[rep].second(rep);

		m_pendingReplies.remove(rep);
		rep->deleteLater();
	});

	return rep;
}
