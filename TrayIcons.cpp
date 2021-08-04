#include "TrayIcons.h"

#include <QTimer>
#include <QSettings>
#include <QMenu>
#include <QInputDialog>
#include <QDesktopServices>

#include <SingleApplication>

#include "ClockifyProject.h"
#include "ClockifyUser.h"
#include "JsonHelper.h"
#include "SelectDefaultProjectDialog.h"

const QByteArray WORKSPACE{"redacted"};
const QByteArray BREAKTIME{"redacted"};

QPair<QString, QIcon> TrayIcons::s_clockifyOn;
QPair<QString, QIcon> TrayIcons::s_clockifyOff;
QPair<QString, QIcon> TrayIcons::s_onBreak;
QPair<QString, QIcon> TrayIcons::s_working;
QPair<QString, QIcon> TrayIcons::s_notWorking;
QPair<QString, QIcon> TrayIcons::s_powerNotConnected;
QPair<QString, QIcon> TrayIcons::s_runningNotConnected;

TrayIcons::TrayIcons(const QSharedPointer<ClockifyUser> &user, QObject *parent)
	: QObject{parent},
	  m_clockifyRunning{new QSystemTrayIcon},
	  m_runningJob{new QSystemTrayIcon},
	  m_user{user}
{
	QSettings settings;
	m_defaultProjectId = settings.value("projectId").toString();

	if (m_defaultProjectId == "")
		getNewProjectId();

	s_clockifyOn = {"Clockify is running", QIcon{":/greenpower.png"}};
	s_clockifyOff = {"Clockify is not running", QIcon{":/redpower.png"}};
	s_onBreak = {"You are on break", QIcon{":/yellowlight.png"}};
	s_working = {"You are working", QIcon{":/greenlight.png"}};
	s_notWorking = {"You are not working", QIcon{":/redlight.png"}};
	s_powerNotConnected = {"You are offline", QIcon{":/graypower.png"}};
	s_runningNotConnected = {"You are offline", QIcon{":/graylight.png"}};

	setUpTrayIcons();

	m_eventLoop.setInterval(500);
	m_eventLoop.setSingleShot(false);
	m_eventLoop.callOnTimeout(this, &TrayIcons::updateTrayIcons);
	m_eventLoop.start();
}

TrayIcons::~TrayIcons()
{
	delete m_clockifyRunning;
	delete m_runningJob;
}

void TrayIcons::show()
{
	m_clockifyRunning->show();
	m_runningJob->show();
}

ClockifyProject TrayIcons::defaultProject() const
{
	if (m_defaultProjectId != "last-entered-code")
		return {m_defaultProjectId, ClockifyManager::instance()->projectName(m_defaultProjectId)};
	else
	{
		if (m_user->hasRunningTimeEntry())
		{
			if (auto project = m_user->getRunningTimeEntry().project(); project.id() != BREAKTIME)
				return project;
		}

		auto entries = m_user->getTimeEntries();
		for (const auto &entry : entries)
		{
			try
			{
				if (entry.project().id() != BREAKTIME)
					return entry.project();
			}
			catch (...)
			{
				std::cerr << "getting project id failed\n";
				continue; // no project id to see here, move along
			}
		}

		// when all else fails, use the first extant project
		return ClockifyManager::instance()->projects().first();
	}
}

void TrayIcons::updateTrayIcons()
{

	if (!ClockifyManager::instance()->isConnectedToInternet())
	{
		setClockifyRunningIconTooltip(s_powerNotConnected);
		setRunningJobIconTooltip(s_runningNotConnected);
	}
	else if (m_user->hasRunningTimeEntry())
	{
		setClockifyRunningIconTooltip(s_clockifyOn);

		try {
			if (m_user->getRunningTimeEntry().project().id() == BREAKTIME)
				setRunningJobIconTooltip(s_onBreak);
			else
				setRunningJobIconTooltip(s_working);
		}
		catch (...)
		{
			std::cerr << "Could not load running time entry\n";
		}
	}
	else
	{
		setClockifyRunningIconTooltip(s_clockifyOff);
		setRunningJobIconTooltip(s_notWorking);
	}
}

void TrayIcons::getNewProjectId()
{
	auto projects = ClockifyManager::instance()->projects();
	QStringList projectIds;
	QStringList projectNames;
	for (const auto &project : projects)
	{
		projectIds.push_back(project.id());
		projectNames.push_back(project.name());
	}

	SelectDefaultProjectDialog dialog{m_defaultProjectId, {projectIds, projectNames}};
	if (dialog.exec() == QDialog::Accepted)
	{
		m_defaultProjectId = dialog.selectedProject();
		QSettings settings;
		settings.setValue("projectId", m_defaultProjectId);
		settings.sync();
	}
}

void TrayIcons::getNewApiKey()
{
	bool ok{true};
	do
	{
		m_apiKey = QInputDialog::getText(nullptr, "API key", "Enter your Clockify API key:", QLineEdit::Normal, m_apiKey, &ok);
		if (!ok)
			return;
	}
	while (m_apiKey == "");

	ClockifyManager::instance()->setApiKey(m_apiKey);

	QSettings settings;
	settings.setValue("apiKey", m_apiKey);
}

void TrayIcons::setUpTrayIcons()
{
	auto m_clockifyRunningMenu = new QMenu;
	auto m_runningJobMenu = new QMenu;

	// set up the menu actions
	connect(m_clockifyRunningMenu->addAction("Start"), &QAction::triggered, this, [&]() {
		if (ClockifyManager::instance()->isConnectedToInternet() == false)
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");

		if (!m_user->hasRunningTimeEntry())
		{
			auto project = defaultProject();
			m_user->startTimeEntry(project.id(), project.description());
			updateTrayIcons();
		}
	});
	connect(m_clockifyRunningMenu->addAction("Stop"), &QAction::triggered, this, [&]() {
		if (ClockifyManager::instance()->isConnectedToInternet() == false)
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");

		if (m_user->hasRunningTimeEntry())
		{
			m_user->stopCurrentTimeEntry();
			updateTrayIcons();
		}
	});
	connect(m_clockifyRunningMenu->addAction("Change default project"), &QAction::triggered, this, &TrayIcons::getNewProjectId);
	connect(m_clockifyRunningMenu->addAction("Change API key"), &QAction::triggered, this, &TrayIcons::getNewApiKey);
	connect(m_clockifyRunningMenu->addAction("Open the Clockify website"), &QAction::triggered, this, []() {
		QDesktopServices::openUrl(QUrl{"https://clockify.me/tracker/"});
	});
	connect(m_clockifyRunningMenu->addAction("Quit"), &QAction::triggered, qApp, &QApplication::quit);

	connect(m_runningJobMenu->addAction("Break"), &QAction::triggered, this, [&]() {
		if (ClockifyManager::instance()->isConnectedToInternet() == false)
		{
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");
			return;
		}

		QDateTime start = QDateTime::currentDateTimeUtc();

		if (m_user->hasRunningTimeEntry())
			start = m_user->stopCurrentTimeEntry();
		m_user->startTimeEntry(BREAKTIME, start);
		updateTrayIcons();
	});
	connect(m_runningJobMenu->addAction("Resume work"), &QAction::triggered, this, [&]() {
		if (ClockifyManager::instance()->isConnectedToInternet() == false)
		{
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");
			return;
		}

		QDateTime start = QDateTime::currentDateTimeUtc();

		if (m_user->hasRunningTimeEntry())
			start = m_user->stopCurrentTimeEntry();
		auto project = defaultProject();
		m_user->startTimeEntry(project.id(), project.description(), start);
		updateTrayIcons();
	});
	connect(m_runningJobMenu->addAction("Change default project"), &QAction::triggered, this, &TrayIcons::getNewProjectId);
	connect(m_runningJobMenu->addAction("Change API key"), &QAction::triggered, this, &TrayIcons::getNewApiKey);
	connect(m_runningJobMenu->addAction("Open the Clockify website"), &QAction::triggered, this, []() {
		QDesktopServices::openUrl(QUrl{"https://clockify.me/tracker/"});
	});
	connect(m_runningJobMenu->addAction("Quit"), &QAction::triggered, qApp, &QApplication::quit);

	// set up the actions on icon click
	connect(m_clockifyRunning, &QSystemTrayIcon::activated, this, [&](QSystemTrayIcon::ActivationReason reason) {
		m_eventLoop.stop();

		if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick)
			return;
		}

		if (ClockifyManager::instance()->isConnectedToInternet() == false)
		{
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");
			m_eventLoop.start();
			return;
		}

		if (m_user->hasRunningTimeEntry())
			m_user->stopCurrentTimeEntry();
		else
		{
			auto project = defaultProject();
			m_user->startTimeEntry(project.id(), project.description());
		}

		m_eventLoop.start();
	});
	connect(m_runningJob, &QSystemTrayIcon::activated, this, [&](QSystemTrayIcon::ActivationReason reason) {
		m_eventLoop.stop();

		if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick)
			return;
		}

		if (ClockifyManager::instance()->isConnectedToInternet() == false)
		{
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");
			m_eventLoop.start();
			return;
		}

		if (m_user->hasRunningTimeEntry())
		{
			if (m_user->getRunningTimeEntry().project().id() == BREAKTIME)
			{
				auto time = m_user->stopCurrentTimeEntry();
				auto project = defaultProject();
				m_user->startTimeEntry(project.id(), project.description(), time);
			}
			else
			{
				auto time = m_user->stopCurrentTimeEntry();
				m_user->startTimeEntry(BREAKTIME, time);
			}
		}
		else
		{
			auto project = defaultProject();
			m_user->startTimeEntry(project.id(), project.description());
		}

		m_eventLoop.start();
	});

	connect(ClockifyManager::instance().data(), &ClockifyManager::internetConnectionChanged, this, [this](bool connected) {
		updateTrayIcons();
	});

	m_clockifyRunning->setContextMenu(m_clockifyRunningMenu);
	m_runningJob->setContextMenu(m_runningJobMenu);

	updateTrayIcons();
}

void TrayIcons::setClockifyRunningIconTooltip(const QPair<QString, QIcon> &data)
{
	m_clockifyRunning->setToolTip(data.first);
	if (m_clockifyRunningCurrentIcon != &data.second)
	{
		m_clockifyRunning->setIcon(data.second);
		m_clockifyRunningCurrentIcon = &data.second;
	}
}

void TrayIcons::setRunningJobIconTooltip(const QPair<QString, QIcon> &data)
{
	m_runningJob->setToolTip(data.first);
	if (m_runningJobCurrentIcon != &data.second)
	{
		m_runningJob->setIcon(data.second);
		m_runningJobCurrentIcon = &data.second;
	}
}
