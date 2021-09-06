#include "ClockifyManager.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QEventLoop>
#include <QNetworkCookieJar>
#include <QNetworkCookie>

#include <iostream>
#include <nlohmann/json.hpp>

#include "JsonHelper.h"
#include "ClockifyUser.h"
#include "ClockifyProject.h"
#include "TimeEntry.h"

using nlohmann::json;

const QString ClockifyManager::s_baseUrl{"https://api.clockify.me/api/v1"};
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

class NoCookies : public QNetworkCookieJar
{
	virtual QList<QNetworkCookie> cookiesForUrl(const QUrl &) const override
	{
		return {};
	}
	virtual bool setCookiesFromUrl(const QList<QNetworkCookie> &, const QUrl &) override
	{
		return false;
	}
};

ClockifyManager::ClockifyManager(QByteArray apiKey, QObject *parent)
	: QObject{parent},
	  m_apiKey{apiKey}
{
	m_manager.setCookieJar(new NoCookies);
	m_manager.setAutoDeleteReplies(true);

	// request currently logged in user (the one whose API key we're using) as a validity test
	// and also in order to cache API key info
	updateCurrentUser();

	m_expireUsersTimer.setInterval(15 * 60 * 1000); // every 15 mins
	m_expireUsersTimer.callOnTimeout(this, [this]() {
		m_usersLoaded = false;
		m_users.clear();
	});
	m_expireUsersTimer.setSingleShot(true);

	m_expireProjectsTimer.setInterval(15 * 60 * 1000);
	m_expireProjectsTimer.callOnTimeout(this, [this]() {
		m_projectsLoaded = false;
		m_projects.clear();
	});
	m_expireProjectsTimer.setSingleShot(true);

	// We won't actually populate the user or project list yet; why do it when we probably won't need it for a long time?

	auto checkInternet = [this] {
		head(QUrl{"https://clockify.me"}, [this](QNetworkReply *) {
			// a changed internet connection is rather unlikely
			if (!m_isConnectedToInternet) [[unlikely]]
			{
				m_isConnectedToInternet = true;
				emit internetConnectionChanged(m_isConnectedToInternet);
			}
		}, [this](QNetworkReply *) {
			if (m_isConnectedToInternet) [[unlikely]]
			{
				m_isConnectedToInternet = false;
				emit internetConnectionChanged(m_isConnectedToInternet);
			}
		});
	};

	checkInternet();

	m_checkConnectionTimer.setInterval(5000); // check connection every 5s, since it's not that likely to change a lot
	m_checkConnectionTimer.callOnTimeout(checkInternet);
	m_checkConnectionTimer.setSingleShot(false);
	m_checkConnectionTimer.start();
}

QVector<ClockifyProject> &ClockifyManager::projects()
{
	while (!m_projectsLoaded)
	{
		if (!m_loadingProjects)
			updateProjects();

		qApp->processEvents();
		qApp->sendPostedEvents();
		if (!m_isValid)
			qApp->quit();
	}

	return m_projects;
}

QVector<QPair<QString, QString>> &ClockifyManager::users()
{
	while (!m_usersLoaded)
	{
		if (!m_loadingUsers)
			updateUsers();

		qApp->processEvents();
		qApp->sendPostedEvents();
		if (!m_isValid)
			qApp->quit();
	}

	return m_users;
}

QString ClockifyManager::projectName(const QString &projectId)
{
	for (auto &item : projects())
		if (item.id() == projectId)
			return item.name();

	return "";
}

QString ClockifyManager::userName(const QString &userId)
{
	for (const auto &item : qAsConst(users()))
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

	get(url, false, [&status](QNetworkReply *rep) {
		// this seems to normally just return empty if there is no running time entry, but I've put in the braces because
		// Clockify seems to like changing behavior (i.e. returning items as arrays sometimes and as objects other times)
		// and because even if Clockify always returns empty, the empty check should not take that long to execute anyway
		if (auto text = rep->readAll(); !text.isEmpty() && text != "[]")
			 status = true;
	});

	return status;
}

QDateTime ClockifyManager::stopRunningTimeEntry(const QString &userId, bool async)
{
	auto now = QDateTime::currentDateTimeUtc();
	json j{{"end", now.toString("yyyy-MM-ddThh:mm:ssZ")}};

	QUrl url{s_baseUrl + "/workspaces/" + m_workspaceId + "/user/" + userId + "/time-entries"};
	patch(url, QByteArray::fromStdString(j.dump()), async);
	return now;
}

TimeEntry ClockifyManager::getRunningTimeEntry(const QString &userId)
{
	QUrl url{s_baseUrl + "/workspaces/" + m_workspaceId + "/user/" + userId + "/time-entries"};
	QUrlQuery query;
	query.addQueryItem("in-progress", "true");
	url.setQuery(query);

	json j;

	get(url, false, [&j](QNetworkReply *rep) {
		try
		{
			j = json::parse(rep->readAll().toStdString());
		}
		catch (...)
		{
			j = {};
		}
	});

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
	post(url, QByteArray::fromStdString(j.dump()), async, 201);
}

// TODO: refactor this to only load small amounts at a time
QVector<TimeEntry> ClockifyManager::getTimeEntries(const QString &userId)
{
	json j;

	get(QUrl{s_baseUrl + "/workspaces/" + m_workspaceId + "/user/" + userId + "/time-entries"}, false, [&j](QNetworkReply *rep) {
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

	QVector<TimeEntry> entries;
	for (const auto &entry : j)
		entries.push_back(TimeEntry{entry});

	return entries;
}

ClockifyUser *ClockifyManager::getApiKeyOwner()
{
	if (!m_ownerId.isEmpty()) [[likely]]
		return new ClockifyUser{m_ownerId, this};
	else
	{
		ClockifyUser *retVal{nullptr};

		get(QUrl{s_baseUrl + "/user"}, false, [this, &retVal](QNetworkReply *rep) {
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

		return retVal;
	}
}

QVector<ClockifyWorkspace> ClockifyManager::getOwnerWorkspaces()
{
	QVector<ClockifyWorkspace> workspaces;

	get(QUrl{s_baseUrl + "/workspaces"}, false, [&workspaces](QNetworkReply *rep) {
		try
		{
			json j{json::parse(rep->readAll().toStdString())};
			for (const auto &workspace : j)
			{
				try
				{
					workspaces.push_back(ClockifyWorkspace{workspace["id"].get<QString>(), workspace["name"].get<QString>()});
				}
				catch (const std::exception &ex)
				{
					std::cerr << "Error while parsing workspace: " << ex.what() << std::endl;
					std::cerr << workspace.dump(4) << std::endl;
				}
			}
		}
		catch (const std::exception &ex)
		{
			std::cerr << "Error while loading workspaces: " << ex.what() << std::endl;
		}
	});

	return workspaces;
}

void ClockifyManager::setApiKey(const QString &apiKey)
{
	m_apiKey = apiKey.toUtf8();
	updateCurrentUser();
	emit apiKeyChanged();
}

void ClockifyManager::setWorkspaceId(const QString &workspaceId)
{
	// TODO: validate this
	m_workspaceId = workspaceId;
}

bool ClockifyManager::init(QByteArray apiKey)
{
	s_instance.reset(new ClockifyManager{apiKey, nullptr});
	return true;
}

void ClockifyManager::updateCurrentUser()
{
	get(s_baseUrl + "/user", false, [this](QNetworkReply *rep) {
		try
		{
			json j{json::parse(rep->readAll().toStdString())};
			if (j.is_array())
			{
				m_ownerId = j[0]["id"].get<QString>();
				m_workspaceId = j[0]["defaultWorkspace"].get<QString>();
			}
			else
			{
				m_ownerId = j["id"].get<QString>();
				m_workspaceId = j["defaultWorkspace"].get<QString>();
			}
			m_isValid = true;
		}
		catch (const std::exception &ex)
		{
			std::cerr << "Error: " << ex.what() << std::endl;
		}
		catch (...)
		{
			std::cout << "Unknown error!\n";
		}
	}, [this](QNetworkReply *rep) {
		if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == 0) [[likely]]
		{
			std::cerr << "Internet connection not found" << std::endl;
			m_isValid = true; // speculative that the connection will work once the internet is connected
			m_isConnectedToInternet = false;
		}
		else [[unlikely]]
		{
			std::cerr << "Request failed with code " << status << std::endl;
			return;
		}
	});
}

void ClockifyManager::updateUsers()
{
	m_loadingUsers = true;

	QUrl url = s_baseUrl + "/workspaces/" + m_workspaceId + "/users";
	QUrlQuery query;
	query.addQueryItem("page-size", QString::number(INT_MAX));
	url.setQuery(query);

	get(url, [this](QNetworkReply *rep) {
		try
		{
			m_usersLoaded = false;
			json j{json::parse(rep->readAll().toStdString())};

			m_users.clear();
			for (const auto &item : j)
			{
				try
				{
					m_users.push_back({item["id"].get<QByteArray>(),
									   item["name"].get<QByteArray>()});
				}
				catch (const std::exception &)
				{
					if (!item.contains("id"))
						throw;
					else // they are guaranteed to have an email address AFAIK
						m_users.push_back({item["id"].get<QByteArray>(), item["email"].get<QByteArray>()});
				}
			}

			m_usersLoaded = true;
			m_expireUsersTimer.start();
		}
		catch (const std::exception &ex)
		{
			m_usersLoaded = false;
		}

		m_loadingUsers = false;
	});
}

void ClockifyManager::updateProjects()
{
	m_loadingProjects = true;

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
			m_expireProjectsTimer.start();
		}
		catch (const std::exception &ex)
		{
			m_projectsLoaded = false;
		}

		m_loadingProjects = false;
	});
}

// *****************************************************************************
// *****     BEYOND THIS POINT THERE ARE ONLY FUNCTIONS FOR REST STUFF     *****
// *****************************************************************************

void ClockifyManager::get(const QUrl &url,
									  const std::function<void (QNetworkReply *)> &successCb,
									  const std::function<void (QNetworkReply *)> &failureCb)
{
	return get(url, true, 200, successCb, failureCb);
}

void ClockifyManager::get(const QUrl &url,
									int expectedReturnCode,
									const std::function<void (QNetworkReply *)> &successCb,
									const std::function<void (QNetworkReply *)> &failureCb)
{
	return get(url, true, expectedReturnCode, successCb, failureCb);
}

void ClockifyManager::get(const QUrl &url,
									bool async,
									const std::function<void (QNetworkReply *)> &successCb,
									const std::function<void (QNetworkReply *)> &failureCb)
{
	return get(url, async, 200, successCb, failureCb);
}

void ClockifyManager::get(const QUrl &url,
									bool async,
									int expectedReturnCode,
									const std::function<void (QNetworkReply *)> &successCb,
									const std::function<void (QNetworkReply *)> &failureCb)
{
	QNetworkRequest req{url};
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setRawHeader("X-Api-Key", m_apiKey);

	auto rep = m_manager.get(req);
	m_pendingReplies.insert(rep, {successCb, failureCb});

	bool *done = async ? nullptr : new bool{false};

	connect(rep, &QNetworkReply::finished, this, [this, rep, expectedReturnCode, done]() {
		if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == expectedReturnCode) [[likely]]
			m_pendingReplies[rep].first(rep);
		else [[unlikely]]
			m_pendingReplies[rep].second(rep);

		if (done != nullptr)
			*done = true;

		m_pendingReplies.remove(rep);
	}, Qt::DirectConnection);

	if (!async)
		while (!(*done))
		{
			qApp->processEvents();
			qApp->sendPostedEvents();
		}

	if (done)
	{
		delete done;
		done = nullptr; // in case we are doing async, this is required to make the lamba's done setting logic work
	}
}

void ClockifyManager::post(const QUrl &url,
									  const QByteArray &body,
									  const std::function<void (QNetworkReply *)> &successCb,
									  const std::function<void (QNetworkReply *)> &failureCb)
{
	return post(url, body, true, 200, successCb, failureCb);
}

void ClockifyManager::post(const QUrl &url,
									 const QByteArray &body,
									 int expectedReturnCode,
									 const std::function<void (QNetworkReply *)> &successCb,
									 const std::function<void (QNetworkReply *)> &failureCb)
{
	return post(url, body, true, expectedReturnCode, successCb, failureCb);
}

void ClockifyManager::post(const QUrl &url,
						   const QByteArray &body,
						   bool async,
						   const std::function<void (QNetworkReply *)> &successCb,
						   const std::function<void (QNetworkReply *)> &failureCb)
{
	return post(url, body, async, 200, successCb, failureCb);
}

void ClockifyManager::post(const QUrl &url,
						   const QByteArray &body,
						   bool async,
						   int expectedReturnCode,
						   const std::function<void (QNetworkReply *)> &successCb,
						   const std::function<void (QNetworkReply *)> &failureCb)
{
	QNetworkRequest req{url};
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setRawHeader("X-Api-Key", m_apiKey);

	auto rep = m_manager.post(req, body);
	m_pendingReplies.insert(rep, {successCb, failureCb});

	bool *done = async ? nullptr : new bool{false};

	connect(rep, &QNetworkReply::finished, this, [this, rep, expectedReturnCode, done]() {
		if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == expectedReturnCode) [[likely]]
			m_pendingReplies[rep].first(rep);
		else [[unlikely]]
			m_pendingReplies[rep].second(rep);

		if (done != nullptr)
			*done = true;

		m_pendingReplies.remove(rep);
	}, Qt::DirectConnection);

	if (!async)
		while (!(*done))
		{
			qApp->processEvents();
			qApp->sendPostedEvents();
		}

	if (done)
	{
		delete done;
		done = nullptr;
	}
}

void ClockifyManager::patch(const QUrl &url,
									  const QByteArray &body,
									  const std::function<void (QNetworkReply *)> &successCb,
									  const std::function<void (QNetworkReply *)> &failureCb)
{
	return patch(url, body, true, 200, successCb, failureCb);
}

void ClockifyManager::patch(const QUrl &url,
									  const QByteArray &body,
									  int expectedReturnCode,
									  const std::function<void (QNetworkReply *)> &successCb,
									  const std::function<void (QNetworkReply *)> &failureCb)
{
	return patch(url, body, true, expectedReturnCode, successCb, failureCb);
}

void ClockifyManager::patch(const QUrl &url,
							const QByteArray &body,
							bool async,
							const std::function<void (QNetworkReply *)> &successCb,
							const std::function<void (QNetworkReply *)> &failureCb)
{
	return patch(url, body, async, 200, successCb, failureCb);
}

void ClockifyManager::patch(const QUrl &url,
							const QByteArray &body,
							bool async,
							int expectedReturnCode,
							const std::function<void (QNetworkReply *)> &successCb,
							const std::function<void (QNetworkReply *)> &failureCb)
{
	QNetworkRequest req{url};
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setRawHeader("X-Api-Key", m_apiKey);

	auto rep = m_manager.sendCustomRequest(req, "PATCH", body);
	m_pendingReplies.insert(rep, {successCb, failureCb});

	bool *done = async ? nullptr : new bool{false};

	connect(rep, &QNetworkReply::finished, this, [this, rep, expectedReturnCode, done]() {
		if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == expectedReturnCode) [[likely]]
			m_pendingReplies[rep].first(rep);
		else [[unlikely]]
			m_pendingReplies[rep].second(rep);

		if (done != nullptr)
			*done = true;

		m_pendingReplies.remove(rep);
	}, Qt::DirectConnection);

	if (!async)
		while (!(*done))
		{
			qApp->processEvents();
			qApp->sendPostedEvents();
		}

	if (done)
	{
		delete done;
		done = nullptr;
	}
}

void ClockifyManager::head(const QUrl &url,
									 const std::function<void (QNetworkReply *)> &successCb,
									 const std::function<void (QNetworkReply *)> &failureCb)
{
	return head(url, true, 200, successCb, failureCb);
}

void ClockifyManager::head(const QUrl &url,
						   int expectedReturnCode,
						   const std::function<void (QNetworkReply *)> &successCb,
						   const std::function<void (QNetworkReply *)> &failureCb)
{
	return head(url, true, expectedReturnCode, successCb, failureCb);
}

void ClockifyManager::head(const QUrl &url,
						   bool async,
						   const std::function<void (QNetworkReply *)> &successCb,
						   const std::function<void (QNetworkReply *)> &failureCb)
{
	return head(url, async, 200, successCb, failureCb);
}

void ClockifyManager::head(const QUrl &url,
						   bool async,
						   int expectedReturnCode,
						   const std::function<void (QNetworkReply *)> &successCb,
						   const std::function<void (QNetworkReply *)> &failureCb)
{
	QNetworkRequest req{url};
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	req.setRawHeader("X-Api-Key", m_apiKey);

	auto rep = m_manager.head(req);
	m_pendingReplies.insert(rep, {successCb, failureCb});

	bool *done = async ? nullptr : new bool{false};

	connect(rep, &QNetworkReply::finished, this, [this, rep, expectedReturnCode, done]() {
		if (auto status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(); status == expectedReturnCode) [[likely]]
			m_pendingReplies[rep].first(rep);
		else [[unlikely]]
			m_pendingReplies[rep].second(rep);

		if (done != nullptr)
			*done = true;

		m_pendingReplies.remove(rep);
	}, Qt::DirectConnection);

	if (!async)
		while (!(*done))
		{
			qApp->processEvents();
			qApp->sendPostedEvents();
		}

	if (done)
	{
		delete done;
		done = nullptr;
	}
}
