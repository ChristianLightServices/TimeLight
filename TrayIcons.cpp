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

QPair<QString, QIcon> TrayIcons::s_clockifyOn;
QPair<QString, QIcon> TrayIcons::s_clockifyOff;
QPair<QString, QIcon> TrayIcons::s_onBreak;
QPair<QString, QIcon> TrayIcons::s_working;
QPair<QString, QIcon> TrayIcons::s_notWorking;
QPair<QString, QIcon> TrayIcons::s_powerNotConnected;
QPair<QString, QIcon> TrayIcons::s_runningNotConnected;

TrayIcons::TrayIcons(QObject *parent)
	: QObject{parent},
	  m_clockifyRunning{new QSystemTrayIcon},
      m_runningJob{new QSystemTrayIcon}
{
	QSettings settings;
	QString apiKey = settings.value("apiKey").toString();

	while (apiKey == QString{})
	{
		bool ok{false};
		apiKey = QInputDialog::getText(nullptr, "API key", "Enter your Clockify API key:", QLineEdit::Normal, QString{}, &ok);
		if (!ok)
		{
			m_valid = false;
			return;
		}
		else
			settings.setValue("apiKey", apiKey);
	}
	m_apiKey = apiKey;
	m_manager = new ClockifyManager{apiKey.toUtf8()};

	auto fixApiKey = [&] {
		while (!m_manager->isValid())
		{
			bool ok{false};
			apiKey = QInputDialog::getText(nullptr,
			                               "API key",
			                               "The API key is incorrect or invalid. Please enter a valid API key:",
			                               QLineEdit::Normal,
			                               QString{},
			                               &ok);
			if (!ok)
				return false;
			settings.setValue("apiKey", apiKey);
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
		QMessageBox::warning(nullptr, QStringLiteral("Fatal error"), QStringLiteral("Could not load user!"));
		m_valid = false;
		return;
	}

	connect(m_manager, &ClockifyManager::apiKeyChanged, this, [this]() {
		auto temp = m_manager->getApiKeyOwner();
		if (temp.isValid()) [[likely]]
		        m_user = temp;
		else [[unlikely]]
		        QMessageBox::warning(nullptr, QStringLiteral("Operation failed"), QStringLiteral("Could not change API key!"));
	});

	m_defaultProjectId = settings.value(QStringLiteral("projectId")).toString();

	m_defaultDescription = settings.value(QStringLiteral("description")).toString();
	m_disableDescription = settings.value(QStringLiteral("disableDescription"), false).toBool();
	m_useLastProject = settings.value(QStringLiteral("useLastProject"), true).toBool();
	m_useLastDescription = settings.value(QStringLiteral("useLastDescription"), true).toBool();
	m_breakTimeId = settings.value(QStringLiteral("breakTimeId")).toString();
	m_eventLoopInterval = settings.value(QStringLiteral("eventLoopInterval"), 500).toInt();
	QString workspaceId = settings.value(QStringLiteral("workspaceId")).toString();

	while (m_defaultProjectId == QString{} && !m_useLastProject)
		getNewProjectId();

	while (workspaceId.isEmpty())
	{
		getNewWorkspaceId();
		workspaceId = m_manager->workspaceId();
	}

	while (m_breakTimeId.isEmpty())
		getNewBreakTimeId();

	s_clockifyOn = {"Clockify is running", QIcon{":/icons/greenpower.png"}};
	s_clockifyOff = {"Clockify is not running", QIcon{":/icons/redpower.png"}};
	s_onBreak = {"You are on break", QIcon{":/icons/yellowlight.png"}};
	s_working = {"You are working", QIcon{":/icons/greenlight.png"}};
	s_notWorking = {"You are not working", QIcon{":/icons/redlight.png"}};
	s_powerNotConnected = {"You are offline", QIcon{":/icons/graypower.png"}};
	s_runningNotConnected = {"You are offline", QIcon{":/icons/graylight.png"}};

	setUpTrayIcons();

	m_eventLoop.setInterval(m_eventLoopInterval);
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

void TrayIcons::setUser(User user)
{
	m_user = user;
}

void TrayIcons::updateTrayIcons()
{
	if (!m_manager->isConnectedToInternet()) [[unlikely]]
	{
		setClockifyRunningIconTooltip(s_powerNotConnected);
		setRunningJobIconTooltip(s_runningNotConnected);
	}
	else if (m_user.hasRunningTimeEntry())
	{
		setClockifyRunningIconTooltip(s_clockifyOn);

		try {
			if (m_user.getRunningTimeEntry().project().id() == m_breakTimeId)
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
		settings.setValue("projectId", m_defaultProjectId);
		settings.setValue("description", m_defaultDescription);
		settings.setValue("disableDescription", m_disableDescription);
		settings.setValue("useLastProject", m_useLastProject);
		settings.setValue("useLastDescription", m_useLastDescription);
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

	m_manager->setApiKey(m_apiKey);

	QSettings settings;
	settings.setValue("apiKey", m_apiKey);
}

void TrayIcons::getNewWorkspaceId()
{
	SelectDefaultWorkspaceDialog dialog{m_manager->getOwnerWorkspaces(), this};
	if (dialog.getNewWorkspace())
	{
		m_manager->setWorkspaceId(dialog.selectedWorkspaceId());

		QSettings settings;
		settings.setValue("workspaceId", dialog.selectedWorkspaceId());
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
											 "Select break time project",
											 "Select the project used for break time:",
											 projectNames,
											 projectIds.indexOf(m_breakTimeId),
											 false,
											 &ok);

	if (ok)
	{
		m_breakTimeId = projectIds[projectNames.indexOf(workspaceId)];

		QSettings settings;
		settings.setValue("breakTimeId", m_breakTimeId);
	}
}

void TrayIcons::showAboutDialog()
{
	// put this into a variable to handle this gonzo string more nicely
	constexpr auto licenseInfo = "ClockifyTrayIcons " VERSION_STR " copyright © 2020. Licensed " \
								 "under the MIT license.\n\nIcons from " \
								 "[1RadicalOne](https://commons.wikimedia.org/wiki/User:1RadicalOne) " \
								 "(light icons, licensed [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/))" \
								 " and [Microsoft](https://github.com/microsoft/fluentui-system-icons) " \
								 "(power icon, licensed [MIT](https://mit-license.org)).";

	QDialog dialog;

	auto layout = new QVBoxLayout{&dialog};

	auto infoLabel = new QLabel{licenseInfo, &dialog};
	infoLabel->setTextFormat(Qt::MarkdownText);
	infoLabel->setWordWrap(true);
	infoLabel->setOpenExternalLinks(true);
	layout->addWidget(infoLabel);

	auto bb = new QDialogButtonBox{QDialogButtonBox::Ok, &dialog};
	connect(bb->addButton("Show license", QDialogButtonBox::ActionRole), &QPushButton::clicked, this, [this, &dialog] {
		showLicenseDialog(&dialog);
	});
	connect(bb, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

	layout->addWidget(bb);

	dialog.setLayout(layout);
	dialog.setWindowTitle("About " + qApp->applicationName());
	dialog.resize(500, dialog.heightForWidth(500));
	dialog.move(dialog.screen()->geometry().width() / 2 - dialog.width() / 2, dialog.screen()->geometry().height() / 2 - dialog.height() / 2);
	dialog.exec();
}

void TrayIcons::showLicenseDialog(QWidget *parent)
{

	QDialog dialog{parent};

	auto layout = new QVBoxLayout{&dialog};

	auto licenseView = new QTextBrowser{&dialog};

	QFile licenseFile{":/LICENSE"};
	if (licenseFile.open(QIODevice::ReadOnly)) [[likely]]
		licenseView->setText(licenseFile.readAll());
	else [[unlikely]] // this really should never happen, but just in case...
		licenseView->setMarkdown("Error: could not load the license. Please read the license on "
								 "[GitHub](https://github.com/ChristianLightServices/ClockifyTrayIcons/blob/master/LICENSE).");

	licenseView->setOpenExternalLinks(true);
	licenseView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	layout->addWidget(licenseView);

	auto bb = new QDialogButtonBox{QDialogButtonBox::Ok, &dialog};
	connect(bb, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

	layout->addWidget(bb);

	dialog.setLayout(layout);
	dialog.setWindowTitle("MIT license");
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
	settings.setValue("eventLoopInterval", m_eventLoopInterval);
}

void TrayIcons::setUpTrayIcons()
{
	auto m_clockifyRunningMenu = new QMenu;
	auto m_runningJobMenu = new QMenu;

	// set up the menu actions
	connect(m_clockifyRunningMenu->addAction("Start"), &QAction::triggered, this, [this]() {
		if (m_manager->isConnectedToInternet() == false) [[unlikely]]
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");

		if (!m_user.hasRunningTimeEntry()) [[likely]]
		{
			auto project = defaultProject();
			m_user.startTimeEntry(project.id(), project.description());
			updateTrayIcons();
		}
	});
	connect(m_clockifyRunningMenu->addAction("Stop"), &QAction::triggered, this, [this]() {
		if (m_manager->isConnectedToInternet() == false) [[unlikely]]
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");

		if (m_user.hasRunningTimeEntry()) [[likely]]
		{
			m_user.stopCurrentTimeEntry();
			updateTrayIcons();
		}
	});
	connect(m_clockifyRunningMenu->addAction("Change default project"), &QAction::triggered, this, &TrayIcons::getNewProjectId);
	connect(m_clockifyRunningMenu->addAction("Change default workspace"), &QAction::triggered, this, &TrayIcons::getNewWorkspaceId);
	connect(m_clockifyRunningMenu->addAction("Change break time project"), &QAction::triggered, this, &TrayIcons::getNewBreakTimeId);
	connect(m_clockifyRunningMenu->addAction("Change API key"), &QAction::triggered, this, &TrayIcons::getNewApiKey);
	connect(m_clockifyRunningMenu->addAction("Change update interval"), &QAction::triggered, this, [this] {
		bool ok{};
		auto interval = QInputDialog::getDouble(nullptr,
												"Change update interval",
												"Choose the new interval in seconds:",
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
	connect(m_clockifyRunningMenu->addAction("Open the Clockify website"), &QAction::triggered, this, []() {
		QDesktopServices::openUrl(QUrl{"https://clockify.me/tracker/"});
	});
	connect(m_clockifyRunningMenu->addAction("About Qt"), &QAction::triggered, this, []() { QMessageBox::aboutQt(nullptr); });
	connect(m_clockifyRunningMenu->addAction("About"), &QAction::triggered, this, &TrayIcons::showAboutDialog);
	connect(m_clockifyRunningMenu->addAction("Quit"), &QAction::triggered, qApp, &QApplication::quit);

	connect(m_runningJobMenu->addAction("Break"), &QAction::triggered, this, [this]() {
		if (m_manager->isConnectedToInternet() == false) [[unlikely]]
		{
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");
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
	connect(m_runningJobMenu->addAction("Resume work"), &QAction::triggered, this, [this]() {
		if (m_manager->isConnectedToInternet() == false) [[unlikely]]
		{
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");
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
	connect(m_runningJobMenu->addAction("Change default project"), &QAction::triggered, this, &TrayIcons::getNewProjectId);
	connect(m_runningJobMenu->addAction("Change default workspace"), &QAction::triggered, this, &TrayIcons::getNewWorkspaceId);
	connect(m_runningJobMenu->addAction("Change break time project"), &QAction::triggered, this, &TrayIcons::getNewBreakTimeId);
	connect(m_runningJobMenu->addAction("Change API key"), &QAction::triggered, this, &TrayIcons::getNewApiKey);
	connect(m_runningJobMenu->addAction("Change update interval"), &QAction::triggered, this, [this] {
		bool ok{};
		auto interval = QInputDialog::getDouble(nullptr,
												"Change update interval",
												"Choose the new interval in seconds:",
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
	connect(m_runningJobMenu->addAction("Open the Clockify website"), &QAction::triggered, this, []() {
		QDesktopServices::openUrl(QUrl{"https://clockify.me/tracker/"});
	});
	connect(m_runningJobMenu->addAction("About Qt"), &QAction::triggered, this, []() { QMessageBox::aboutQt(nullptr); });
	connect(m_runningJobMenu->addAction("About"), &QAction::triggered, this, &TrayIcons::showAboutDialog);
	connect(m_runningJobMenu->addAction("Quit"), &QAction::triggered, qApp, &QApplication::quit);

	// set up the actions on icon click
	connect(m_clockifyRunning, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
		m_eventLoop.stop();

		if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick) [[unlikely]]
		{
			m_eventLoop.start();
			return;
		}

		if (m_manager->isConnectedToInternet() == false) [[unlikely]]
		{
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");
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
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");
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

	m_clockifyRunning->setContextMenu(m_clockifyRunningMenu);
	m_runningJob->setContextMenu(m_runningJobMenu);

	updateTrayIcons();
}

void TrayIcons::setClockifyRunningIconTooltip(const QPair<QString, QIcon> &data)
{
	if (m_clockifyRunningCurrentTooltip != data.first) [[unlikely]]
	{
		m_clockifyRunning->setToolTip(data.first);
		m_clockifyRunningCurrentTooltip = data.first;
	}
	if (m_clockifyRunningCurrentIcon != &data.second) [[unlikely]]
	{
		m_clockifyRunning->setIcon(data.second);
		m_clockifyRunningCurrentIcon = &data.second;
	}
}

void TrayIcons::setRunningJobIconTooltip(const QPair<QString, QIcon> &data)
{
	if (m_runningJobCurrentTooltip != data.first) [[unlikely]]
	{
		m_runningJob->setToolTip(data.first);
		m_runningJobCurrentTooltip = data.first;
	}
	if (m_runningJobCurrentIcon != &data.second) [[unlikely]]
	{
		m_runningJob->setIcon(data.second);
		m_runningJobCurrentIcon = &data.second;

		// we'll use this icon as the window icon
		qApp->setWindowIcon(data.second);
	}
}
