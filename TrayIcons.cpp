#include "TrayIcons.h"

#include <QTimer>
#include <QSettings>
#include <QMenu>
#include <QInputDialog>
#include <QDesktopServices>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QTextBrowser>
#include <QMessageBox>
#include <QFile>
#include <QFontDatabase>
#include <QScreen>

#include <SingleApplication>

#include "ClockifyManager.h"
#include "Project.h"
#include "User.h"
#include "JsonHelper.h"
#include "SelectDefaultProjectDialog.h"
#include "SelectDefaultWorkspaceDialog.h"
#include "version.h"

QPair<QString, QIcon> TrayIcons::s_timerOn;
QPair<QString, QIcon> TrayIcons::s_timerOff;
QPair<QString, QIcon> TrayIcons::s_onBreak;
QPair<QString, QIcon> TrayIcons::s_working;
QPair<QString, QIcon> TrayIcons::s_notWorking;
QPair<QString, QIcon> TrayIcons::s_powerNotConnected;
QPair<QString, QIcon> TrayIcons::s_runningNotConnected;

TrayIcons::TrayIcons(QObject *parent)
	: QObject{parent},
      m_timerRunning{new QSystemTrayIcon},
      m_runningJob{new QSystemTrayIcon}
{
	QSettings settings;
	QString apiKey = settings.value(QStringLiteral("apiKey")).toString();

	while (apiKey == QString{})
	{
		bool ok{false};
		apiKey = QInputDialog::getText(nullptr, tr("API key"), tr("Enter your Clockify API key:"), QLineEdit::Normal, QString{}, &ok);
		if (!ok)
		{
			m_valid = false;
			return;
		}
		else
			settings.setValue(QStringLiteral("apiKey"), apiKey);
	}
	m_apiKey = apiKey;
	m_manager = new ClockifyManager{apiKey.toUtf8()};

	auto fixApiKey = [&] {
		while (!m_manager->isValid())
		{
			bool ok{false};
			apiKey = QInputDialog::getText(nullptr,
			                               tr("API key"),
			                               tr("The API key is incorrect or invalid. Please enter a valid API key:"),
			                               QLineEdit::Normal,
			                               QString{},
			                               &ok);
			if (!ok)
				return false;
			settings.setValue(QStringLiteral("apiKey"), apiKey);
			m_manager->setApiKey(apiKey);
		}

		return true;
	};
	if (!fixApiKey())
	{
		m_valid = false;
		return;
	}

	connect(m_manager, &ClockifyManager::invalidated, this, [this, fixApiKey]() {
		if (!fixApiKey())
		{
			m_valid = false;
			return;
		}
	});

	m_user = m_manager->getApiKeyOwner();
	m_manager->setWorkspaceId(settings.value(QStringLiteral("workspaceId"), m_user.workspaceId()).toString());
	if (!m_user.isValid()) [[unlikely]]
	{
		QMessageBox::warning(nullptr, tr("Fatal error"), tr("Could not load user!"));
		m_valid = false;
		return;
	}

	connect(m_manager, &ClockifyManager::apiKeyChanged, this, [this]() {
		auto temp = m_manager->getApiKeyOwner();
		if (temp.isValid()) [[likely]]
		        m_user = temp;
		else [[unlikely]]
		        QMessageBox::warning(nullptr, tr("Operation failed"), tr("Could not change API key!"));
	});

	m_defaultProjectId = settings.value(QStringLiteral("projectId")).toString();

	m_defaultDescription = settings.value(QStringLiteral("description")).toString();
	m_disableDescription = settings.value(QStringLiteral("disableDescription"), false).toBool();
	m_useLastProject = settings.value(QStringLiteral("useLastProject"), true).toBool();
	m_useLastDescription = settings.value(QStringLiteral("useLastDescription"), true).toBool();
	m_breakTimeId = settings.value(QStringLiteral("breakTimeId")).toString();
	m_eventLoopInterval = settings.value(QStringLiteral("eventLoopInterval"), 500).toInt();
	QString workspaceId = settings.value(QStringLiteral("workspaceId")).toString();
	m_showDurationNotifications = settings.value(QStringLiteral("showDurationNotifications"), true).toBool();

	while (m_defaultProjectId == QString{} && !m_useLastProject)
		getNewProjectId();

	while (workspaceId.isEmpty())
	{
		getNewWorkspaceId();
		workspaceId = m_manager->workspaceId();
	}

	while (m_breakTimeId.isEmpty())
		getNewBreakTimeId();

	s_timerOn = {tr("Clockify is running"), QIcon{":/icons/greenpower.png"}};
	s_timerOff = {tr("Clockify is not running"), QIcon{":/icons/redpower.png"}};
	s_onBreak = {tr("You are on break"), QIcon{":/icons/yellowlight.png"}};
	s_working = {tr("You are working"), QIcon{":/icons/greenlight.png"}};
	s_notWorking = {tr("You are not working"), QIcon{":/icons/redlight.png"}};
	s_powerNotConnected = {tr("You are offline"), QIcon{":/icons/graypower.png"}};
	s_runningNotConnected = {tr("You are offline"), QIcon{":/icons/graylight.png"}};

	setUpTrayIcons();

	m_eventLoop.setInterval(m_eventLoopInterval);
	m_eventLoop.setSingleShot(false);
	m_eventLoop.callOnTimeout(this, &TrayIcons::updateTrayIcons);
	m_eventLoop.start();
}

TrayIcons::~TrayIcons()
{
	delete m_timerRunning;
	delete m_runningJob;
}

void TrayIcons::show()
{
	m_timerRunning->show();
	m_runningJob->show();
}

Project TrayIcons::defaultProject()
{
	QString projectId;
	QString description;
	int pageNum{m_manager->paginationStartsAt()};
	bool itemsLeftToLoad{true};
	auto entries = m_user.getTimeEntries(pageNum);

	if (!m_useLastProject)
		projectId = m_defaultProjectId;
	else
	{
		bool projectIdLoaded{false};

		do
		{
			for (const auto &entry : entries)
			{
				if (entry.project().id().isEmpty()) [[unlikely]]
				{
					std::cerr << "getting project id failed\n";
					continue; // no project id to see here, move along
				}
				else if (entry.project().id() != m_breakTimeId)
				{
					projectId = entry.project().id();
					projectIdLoaded = true;
					break;
				}
			}

			auto newEntries = m_user.getTimeEntries(++pageNum);
			if (newEntries.empty())
				itemsLeftToLoad = false;
			else
				entries.append(newEntries);
		} while (itemsLeftToLoad || !projectIdLoaded);

		// when all else fails, use the first extant project
		if (!projectIdLoaded) [[unlikely]]
		    projectId = m_manager->projects().first().id();
	}

	if (m_disableDescription)
		; // description is already empty, so we will fall through
	else if (!m_useLastDescription)
		description = m_defaultDescription;
	else
	{
		bool descriptionLoaded{false};

		do
		{
			for (const auto &entry : entries)
			{
				if (entry.project().id() != m_breakTimeId)
				{
					description = entry.project().description();
					descriptionLoaded = true;
					break;
				}
			}

			auto newEntries = m_user.getTimeEntries(++pageNum);
			if (newEntries.empty())
				itemsLeftToLoad = false;
			else
				entries.append(newEntries);
		} while (itemsLeftToLoad || !descriptionLoaded);

		if (!descriptionLoaded) [[unlikely]]
		    description = m_manager->projects().first().description();
	}

	return Project{projectId, m_manager->projectName(projectId), description};
}

void TrayIcons::setUser(const User &user)
{
	m_user = user;
}

void TrayIcons::updateTrayIcons()
{
	if (!m_manager->isConnectedToInternet()) [[unlikely]]
	    setTimerState(TimerState::Offline);
	else if (m_user.hasRunningTimeEntry())
	{
		try
		{
			if (m_user.getRunningTimeEntry().project().id() == m_breakTimeId)
				setTimerState(TimerState::OnBreak);
			else
				setTimerState(TimerState::Running);
		}
		catch (const std::exception &ex)
		{
			std::cerr << "Could not load running time entry: " << ex.what() << std::endl;
		}
		catch (...)
		{
			std::cerr << "Could not load running time entry" << std::endl;
		}
	}
	else
		setTimerState(TimerState::NotRunning);
}

void TrayIcons::getNewProjectId()
{
	QStringList projectIds;
	QStringList projectNames;
	for (auto &project : m_manager->projects())
	{
		projectIds.push_back(project.id());
		projectNames.push_back(project.name());
	}

	SelectDefaultProjectDialog dialog{m_useLastProject,
									  m_useLastDescription,
									  m_disableDescription,
									  m_defaultProjectId,
									  m_defaultDescription,
									  {projectIds, projectNames}};
	if (dialog.exec() == QDialog::Accepted)
	{
		m_defaultProjectId = dialog.selectedProject();
		m_defaultDescription = dialog.selectedDescription();
		m_disableDescription = dialog.disableDescription();
		m_useLastProject = dialog.useLastProject();
		m_useLastDescription = dialog.useLastDescription();

		QSettings settings;
		settings.setValue(QStringLiteral("projectId"), m_defaultProjectId);
		settings.setValue(QStringLiteral("description"), m_defaultDescription);
		settings.setValue(QStringLiteral("disableDescription"), m_disableDescription);
		settings.setValue(QStringLiteral("useLastProject"), m_useLastProject);
		settings.setValue(QStringLiteral("useLastDescription"), m_useLastDescription);
		settings.sync();
	}
}

void TrayIcons::getNewApiKey()
{
	bool ok{true};
	do
	{
		m_apiKey = QInputDialog::getText(nullptr, tr("API key"), tr("Enter your Clockify API key:"), QLineEdit::Normal, m_apiKey, &ok);
		if (!ok)
			return;
	}
	while (m_apiKey == "");

	m_manager->setApiKey(m_apiKey);

	QSettings settings;
	settings.setValue(QStringLiteral("apiKey"), m_apiKey);
}

void TrayIcons::getNewWorkspaceId()
{
	SelectDefaultWorkspaceDialog dialog{m_manager->getOwnerWorkspaces(), this};
	if (dialog.getNewWorkspace())
	{
		m_manager->setWorkspaceId(dialog.selectedWorkspaceId());

		QSettings settings;
		settings.setValue(QStringLiteral("workspaceId"), dialog.selectedWorkspaceId());
	}
}

void TrayIcons::getNewBreakTimeId()
{
	auto ok{false};
	auto projects = m_manager->projects();

	QStringList projectIds;
	QStringList projectNames;

	for (const auto &project : qAsConst(projects))
	{
		projectIds.push_back(project.id());
		projectNames.push_back(project.name());
	}

	auto workspaceId = QInputDialog::getItem(nullptr,
	                                         tr("Select break time project"),
	                                         tr("Select the project used for break time:"),
											 projectNames,
											 projectIds.indexOf(m_breakTimeId),
											 false,
											 &ok);

	if (ok)
	{
		m_breakTimeId = projectIds[projectNames.indexOf(workspaceId)];

		QSettings settings;
		settings.setValue(QStringLiteral("breakTimeId"), m_breakTimeId);
	}
}

void TrayIcons::showAboutDialog()
{
	// put this into a variable to handle this gonzo string more nicely
	auto licenseInfo = tr("ClockifyTrayIcons %1 copyright © 2020. Licensed " \
								 "under the MIT license.\n\nIcons from " \
								 "[1RadicalOne](https://commons.wikimedia.org/wiki/User:1RadicalOne) " \
								 "(light icons, licensed [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/))" \
								 " and [Microsoft](https://github.com/microsoft/fluentui-system-icons) " \
	                             "(power icon, licensed [MIT](https://mit-license.org)).").arg(VERSION_STR);

	QDialog dialog;

	auto layout = new QVBoxLayout{&dialog};

	auto infoLabel = new QLabel{licenseInfo, &dialog};
	infoLabel->setTextFormat(Qt::MarkdownText);
	infoLabel->setWordWrap(true);
	infoLabel->setOpenExternalLinks(true);
	layout->addWidget(infoLabel);

	auto bb = new QDialogButtonBox{QDialogButtonBox::Ok, &dialog};
	connect(bb->addButton(tr("Show license"), QDialogButtonBox::ActionRole), &QPushButton::clicked, this, [this, &dialog] {
		showLicenseDialog(&dialog);
	});
	connect(bb, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

	layout->addWidget(bb);

	dialog.setLayout(layout);
	dialog.setWindowTitle(tr("About ") + qApp->applicationName());
	dialog.resize(500, dialog.heightForWidth(500));
	dialog.move(dialog.screen()->geometry().width() / 2 - dialog.width() / 2, dialog.screen()->geometry().height() / 2 - dialog.height() / 2);
	dialog.exec();
}

void TrayIcons::showLicenseDialog(QWidget *parent)
{

	QDialog dialog{parent};

	auto layout = new QVBoxLayout{&dialog};

	auto licenseView = new QTextBrowser{&dialog};

	QFile licenseFile{QStringLiteral(":/LICENSE")};
	if (licenseFile.open(QIODevice::ReadOnly)) [[likely]]
		licenseView->setText(licenseFile.readAll());
	else [[unlikely]] // this really should never happen, but just in case...
	    licenseView->setMarkdown(tr("Error: could not load the license. Please read the license on "
	                             "[GitHub](https://github.com/ChristianLightServices/ClockifyTrayIcons/blob/master/LICENSE)."));

	licenseView->setOpenExternalLinks(true);
	licenseView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	layout->addWidget(licenseView);

	auto bb = new QDialogButtonBox{QDialogButtonBox::Ok, &dialog};
	connect(bb, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

	layout->addWidget(bb);

	dialog.setLayout(layout);
	dialog.setWindowTitle(tr("MIT license"));
	dialog.setModal(true);
	dialog.resize(600, 600);
	dialog.move(dialog.screen()->geometry().width() / 2 - dialog.width() / 2, dialog.screen()->geometry().height() / 2 - dialog.height() / 2);
	dialog.exec();
}

void TrayIcons::setEventLoopInterval(int interval)
{
	m_eventLoopInterval = interval;
	m_eventLoop.setInterval(m_eventLoopInterval);

	QSettings settings;
	settings.setValue(QStringLiteral("eventLoopInterval"), m_eventLoopInterval);
}

void TrayIcons::setUpTrayIcons()
{
	auto m_timerRunningMenu = new QMenu;
	auto m_runningJobMenu = new QMenu;

	// set up the menu actions
	connect(m_timerRunningMenu->addAction(tr("Start")), &QAction::triggered, this, [this]() {
		if (m_manager->isConnectedToInternet() == false) [[unlikely]]
		        m_timerRunning->showMessage(tr("Internet connection lost"), tr("The request could not be completed because the internet connection is down."));

		if (!m_user.hasRunningTimeEntry()) [[likely]]
		{
			auto project = defaultProject();
			m_user.startTimeEntry(project.id(), project.description());
			updateTrayIcons();
		}
	});
	connect(m_timerRunningMenu->addAction(tr("Stop")), &QAction::triggered, this, [this]() {
		if (m_manager->isConnectedToInternet() == false) [[unlikely]]
		    m_timerRunning->showMessage(tr("Internet connection lost"), tr("The request could not be completed because the internet connection is down."));

		if (m_user.hasRunningTimeEntry()) [[likely]]
		{
			m_user.stopCurrentTimeEntry();
			updateTrayIcons();
		}
	});
	connect(m_timerRunningMenu->addAction(tr("Change default project")), &QAction::triggered, this, &TrayIcons::getNewProjectId);
	connect(m_timerRunningMenu->addAction(tr("Change default workspace")), &QAction::triggered, this, &TrayIcons::getNewWorkspaceId);
	connect(m_timerRunningMenu->addAction(tr("Change break time project")), &QAction::triggered, this, &TrayIcons::getNewBreakTimeId);
	connect(m_timerRunningMenu->addAction(tr("Change API key")), &QAction::triggered, this, &TrayIcons::getNewApiKey);
	connect(m_timerRunningMenu->addAction(tr("Change update interval")), &QAction::triggered, this, [this] {
		bool ok{};
		auto interval = QInputDialog::getDouble(nullptr,
		                                        tr("Change update interval"),
		                                        tr("Choose the new interval in seconds:"),
												static_cast<double>(m_eventLoopInterval) / 1000,
												0,
												10,
												1,
												&ok,
												Qt::WindowFlags{},
												0.5);

		if (ok)
			setEventLoopInterval(interval * 1000);
	});
	{
		auto notifs = m_timerRunningMenu->addAction(tr("Show job duration at completion"));
		notifs->setCheckable(true);
		notifs->setChecked(m_showDurationNotifications);
		connect(notifs, &QAction::triggered, this, [this, notifs] {
			m_showDurationNotifications = notifs->isChecked();
			QSettings settings;
			settings.setValue(QStringLiteral("showDurationNotifications"), m_showDurationNotifications);
		});
	}
	connect(m_timerRunningMenu->addAction(tr("Open the Clockify website")), &QAction::triggered, this, []() {
		QDesktopServices::openUrl(QUrl{QStringLiteral("https://clockify.me/tracker/")});
	});
	connect(m_timerRunningMenu->addAction(tr("About Qt")), &QAction::triggered, this, []() { QMessageBox::aboutQt(nullptr); });
	connect(m_timerRunningMenu->addAction(tr("About")), &QAction::triggered, this, &TrayIcons::showAboutDialog);
	connect(m_timerRunningMenu->addAction(tr("Quit")), &QAction::triggered, qApp, &QApplication::quit);

	connect(m_runningJobMenu->addAction(tr("Break")), &QAction::triggered, this, [this]() {
		if (m_manager->isConnectedToInternet() == false) [[unlikely]]
		{
			m_timerRunning->showMessage(tr("Internet connection lost"), tr("The request could not be completed because the internet connection is down."));
			return;
		}

		QDateTime start = QDateTime::currentDateTimeUtc();

		if (m_user.hasRunningTimeEntry())
		{
			try
			{
				if (m_user.getRunningTimeEntry().project().id() == m_breakTimeId)
					return;
			}
			catch (const std::exception &) {}

			start = m_user.stopCurrentTimeEntry();
		}
		m_user.startTimeEntry(m_breakTimeId, start);
		updateTrayIcons();
	});
	connect(m_runningJobMenu->addAction(tr("Resume work")), &QAction::triggered, this, [this]() {
		if (m_manager->isConnectedToInternet() == false) [[unlikely]]
		{
			m_timerRunning->showMessage(tr("Internet connection lost"), tr("The request could not be completed because the internet connection is down."));
			return;
		}

		QDateTime start = QDateTime::currentDateTimeUtc();

		if (m_user.hasRunningTimeEntry())
		{
			try
			{
				if (m_user.getRunningTimeEntry().project().id() != m_breakTimeId)
					return;
			}
			catch (const std::exception &) {}

			start = m_user.stopCurrentTimeEntry();
		}
		auto project = defaultProject();
		m_user.startTimeEntry(project.id(), project.description(), start);
		updateTrayIcons();
	});
	connect(m_runningJobMenu->addAction(tr("Change default project")), &QAction::triggered, this, &TrayIcons::getNewProjectId);
	connect(m_runningJobMenu->addAction(tr("Change default workspace")), &QAction::triggered, this, &TrayIcons::getNewWorkspaceId);
	connect(m_runningJobMenu->addAction(tr("Change break time project")), &QAction::triggered, this, &TrayIcons::getNewBreakTimeId);
	connect(m_runningJobMenu->addAction(tr("Change API key")), &QAction::triggered, this, &TrayIcons::getNewApiKey);
	connect(m_runningJobMenu->addAction(tr("Change update interval")), &QAction::triggered, this, [this] {
		bool ok{};
		auto interval = QInputDialog::getDouble(nullptr,
		                                        tr("Change update interval"),
		                                        tr("Choose the new interval in seconds:"),
												static_cast<double>(m_eventLoopInterval) / 1000,
												0,
												10,
												1,
												&ok,
												Qt::WindowFlags{},
												0.5);

		if (ok)
			setEventLoopInterval(interval * 1000);
	});
	{
		auto notifs = m_runningJobMenu->addAction(tr("Show job duration at completion"));
		notifs->setCheckable(true);
		notifs->setChecked(m_showDurationNotifications);
		connect(notifs, &QAction::triggered, this, [this, notifs] {
			m_showDurationNotifications = notifs->isChecked();
			QSettings settings;
			settings.setValue(QStringLiteral("showDurationNotifications"), m_showDurationNotifications);
		});
	}
	connect(m_runningJobMenu->addAction(tr("Open the Clockify website")), &QAction::triggered, this, []() {
		QDesktopServices::openUrl(QUrl{QStringLiteral("https://clockify.me/tracker/")});
	});
	connect(m_runningJobMenu->addAction(tr("About Qt")), &QAction::triggered, this, []() { QMessageBox::aboutQt(nullptr); });
	connect(m_runningJobMenu->addAction(tr("About")), &QAction::triggered, this, &TrayIcons::showAboutDialog);
	connect(m_runningJobMenu->addAction(tr("Quit")), &QAction::triggered, qApp, &QApplication::quit);

	// set up the actions on icon click
	connect(m_timerRunning, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
		m_eventLoop.stop();

		if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick) [[unlikely]]
		{
			m_eventLoop.start();
			return;
		}

		if (m_manager->isConnectedToInternet() == false) [[unlikely]]
		{
			m_timerRunning->showMessage(tr("Internet connection lost"), tr("The request could not be completed because the internet connection is down."));
			m_eventLoop.start();
			return;
		}

		if (m_user.hasRunningTimeEntry())
			m_user.stopCurrentTimeEntry();
		else
		{
			auto project = defaultProject();
			m_user.startTimeEntry(project.id(), project.description());
		}

		m_eventLoop.start();
	});
	connect(m_runningJob, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
		m_eventLoop.stop();

		if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick) [[unlikely]]
		{
			m_eventLoop.start();
			return;
		}

		if (m_manager->isConnectedToInternet() == false) [[unlikely]]
		{
			m_timerRunning->showMessage(tr("Internet connection lost"), tr("The request could not be completed because the internet connection is down."));
			m_eventLoop.start();
			return;
		}

		if (m_user.hasRunningTimeEntry())
		{
			if (m_user.getRunningTimeEntry().project().id() == m_breakTimeId)
			{
				auto time = m_user.stopCurrentTimeEntry();
				auto project = defaultProject();
				m_user.startTimeEntry(project.id(), project.description(), time);
			}
			else
			{
				auto time = m_user.stopCurrentTimeEntry();
				m_user.startTimeEntry(m_breakTimeId, time);
			}
		}
		else
		{
			auto project = defaultProject();
			m_user.startTimeEntry(project.id(), project.description());
		}

		m_eventLoop.start();
	});

	connect(m_manager, &ClockifyManager::internetConnectionChanged, this, [this](bool connected) {
		updateTrayIcons();
	});

	m_timerRunning->setContextMenu(m_timerRunningMenu);
	m_runningJob->setContextMenu(m_runningJobMenu);

	connect(this, &TrayIcons::jobEnded, this, [this] {
		if (!m_showDurationNotifications)
			return;

		auto jobs = m_user.getTimeEntries(1, 1);
		if (jobs.isEmpty())
			return;
		auto job{jobs.first()};

		QTime duration{QTime::fromMSecsSinceStartOfDay(job.start().msecsTo(job.end()))};
		QString timeString{tr("%n minute(s)", nullptr, duration.minute())};
		if (duration.hour() > 0)
			timeString.prepend(tr("%n hour(s) and ", nullptr, duration.hour()));
		else
			timeString.append(tr(" and %n second(s)", nullptr, duration.second()));
		m_timerRunning->showMessage(tr("Job ended"),
		                            tr("You worked %1 on %2")
		                                .arg(timeString, job.project().name()),
		                            QSystemTrayIcon::Information,
		                            5000);
	});

	updateTrayIcons();
}

void TrayIcons::setTimerState(const TimerState state)
{
	if (state == m_timerState || state == TimerState::StateUnset)
		return;

	switch (state)
	{
	case TimerState::Running:
		m_timerRunning->setToolTip(s_timerOn.first);
		m_timerRunning->setIcon(s_timerOn.second);
		m_runningJob->setToolTip(s_working.first);
		m_runningJob->setIcon(s_working.second);
		break;
	case TimerState::OnBreak:
		m_timerRunning->setToolTip(s_timerOn.first);
		m_timerRunning->setIcon(s_timerOn.second);
		m_runningJob->setToolTip(s_onBreak.first);
		m_runningJob->setIcon(s_onBreak.second);

		if (m_timerState == TimerState::Running)
			emit jobEnded();
		break;
	case TimerState::NotRunning:
		m_timerRunning->setToolTip(s_timerOff.first);
		m_timerRunning->setIcon(s_timerOff.second);
		m_runningJob->setToolTip(s_notWorking.first);
		m_runningJob->setIcon(s_notWorking.second);

		if (m_timerState == TimerState::Running)
			emit jobEnded();
		break;
	case TimerState::Offline:
		m_timerRunning->setToolTip(s_powerNotConnected.first);
		m_timerRunning->setIcon(s_powerNotConnected.second);
		m_runningJob->setToolTip(s_runningNotConnected.first);
		m_runningJob->setIcon(s_runningNotConnected.second);
		break;
	default:
		break;
	}

	m_timerState = state;
	emit timerStateChanged();
}
