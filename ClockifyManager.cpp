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

using nlohmann::json;

const QByteArray ClockifyManager::s_baseUrl{"https://api.clockify.me/api/v1"};
const std::function<void (QNetworkReply *)> ClockifyManager::s_defaultSuccessCb = [](QNetworkReply *){};
const std::function<void (QNetworkReply *)> ClockifyManager::s_defaultFailureCb = [](QNetworkReply *reply) {
	std::cerr << "Request " << reply->url().toString().toStdString()
			  << " failed with code "
			  << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
			  << std::endl;
};

ClockifyManager::ClockifyManager(QByteArray workspaceId, QByteArray apiKey, QObject *parent)
	: QObject{parent},
	  m_workspaceId{workspaceId},
	  m_apiKey{apiKey}
{
	// request currently logged in user (the one whose API key we're using) as a validity test
	QUrl url{s_baseUrl + "/user"};

	QNetworkRequest currentUser{url};
	currentUser.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	currentUser.setRawHeader("X-Api-Key", m_apiKey);

	auto userRep = m_manager.get(currentUser);
	QEventLoop loop;
	connect(userRep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	if (auto status = userRep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == 200)
		m_isValid = true;
	else
	{
		std::cerr << "Request failed with code " << status << std::endl;
		return;
	}

	// get all projects for project list (note that this will return immediately and finish running in the
	// background; when the user calls projects(), projects() will block if needed until the projects are loaded
	url = s_baseUrl + "/workspaces/" + m_workspaceId + "/projects";

	// let's get all the projects! *evil laughter*
	QUrlQuery query;
	query.addQueryItem("page-size", QString::number(INT_MAX));
	url.setQuery(query);

	get(url, [this](QNetworkReply *rep) {
		json j{json::parse(rep->readAll().toStdString())};

		for (auto item : j)
			m_projects.push_back({item["id"].get<QByteArray>(),
								  item["name"].get<QByteArray>()});

		m_projectsLoaded = true;
		emit projectsLoaded();
	});

	// now load the users
	url = s_baseUrl + "/workspaces/" + m_workspaceId + "/users";
	query.clear();
	query.addQueryItem("page-size", QString::number(INT_MAX));
	url.setQuery(query);

	get(url, [this](QNetworkReply *rep) {
		json j{json::parse(rep->readAll().toStdString())};

		for (auto item : j)
			m_users.push_back({item["id"].get<QByteArray>(),
							   item["name"].get<QByteArray>()});

		m_usersLoaded = true;
		emit usersLoaded();
	});
}

QList<QPair<QString, QString>> ClockifyManager::projects()
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

	for (auto item : m_projects)
		if (item.first == projectId)
			return item.second;

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

	for (auto item : m_users)
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

	auto reply = get(url);
	QEventLoop loop;
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	if (auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == 200)
	{
		json j{json::parse(reply->readAll().toStdString())};
		if (!j.empty())
			return true;
	}

	return false;
}

void ClockifyManager::stopRunningTimeEntry(const QString &userId, bool async)
{
	json j{{"end", QDateTime::currentDateTime().toUTC().toString("yyyy-MM-ddThh:mm:ssZ")}};

	QUrl url{s_baseUrl + "/workspaces/" + m_workspaceId + "/user/" + userId + "/time-entries"};
	auto rep = patch(url, QByteArray::fromStdString(j.dump()));

	if (!async)
	{
		QEventLoop loop;
		connect(rep, &QNetworkReply::finished, &loop, &QEventLoop::quit);
		loop.exec();
	}
}

json ClockifyManager::getRunningTimeEntry(const QString &userId)
{
	QUrl url{s_baseUrl + "/workspaces/" + m_workspaceId + "/user/" + userId + "/time-entries"};
	QUrlQuery query;
	query.addQueryItem("in-progress", "true");
	url.setQuery(query);

	auto reply = get(url);
	QEventLoop loop;
	connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();

	if (auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == 200)
		return json::parse(reply->readAll().toStdString());

	return {};
}

void ClockifyManager::startTimeEntry(const QString &userId, const QString &projectId, bool async)
{
	startTimeEntry(userId, projectId, QString{}, async);
}

void ClockifyManager::startTimeEntry(const QString &userId, const QString &projectId, const QString &description, bool async)
{
	json j{
		{"start", QDateTime::currentDateTime().toUTC().toString("yyyy-MM-ddThh:mm:ssZ")},
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
	});

	return rep;
}