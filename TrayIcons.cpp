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

#include "ClockifyProject.h"
#include "ClockifyUser.h"
#include "JsonHelper.h"
#include "SelectDefaultProjectDialog.h"

const auto WORKSPACE{"5e207622ba05e2483903da0d"};
const auto TIME_301{"5e20d8e0ba05e2483904a186"};

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
	  m_user{user},
	  m_apiKey{ClockifyManager::instance()->apiKey()}
{
	QSettings settings;
	m_defaultProjectId = settings.value("projectId").toString();

	// migration for the old default project id logic
	// TODO: remove once everybody has been migrated
	if (m_defaultProjectId == "last-entered-code") [[likely]]
	{
		settings.setValue("useLastProject", true);
		settings.setValue("useLastDescription", true);

		m_defaultProjectId.clear();
		settings.setValue("projectId", m_defaultProjectId);
	}
	else if (!m_defaultProjectId.isEmpty() && !settings.allKeys().contains("useLastProject")) [[likely]]
	{
		settings.setValue("useLastProject", false);
		settings.setValue("useLastDescription", false);
	}
	// end of migration

	m_defaultDescription = settings.value("description").toString();
	m_disableDescription = settings.value("disableDescription", false).toBool();
	m_useLastProject = settings.value("useLastProject", true).toBool();
	m_useLastDescription = settings.value("useLastDescription", true).toBool();

	if (m_defaultProjectId == "" && !m_useLastProject)
		getNewProjectId();

	s_clockifyOn = {"Clockify is running", QIcon{":/icons/greenpower.png"}};
	s_clockifyOff = {"Clockify is not running", QIcon{":/icons/redpower.png"}};
	s_onBreak = {"You are on break", QIcon{":/icons/yellowlight.png"}};
	s_working = {"You are working", QIcon{":/icons/greenlight.png"}};
	s_notWorking = {"You are not working", QIcon{":/icons/redlight.png"}};
	s_powerNotConnected = {"You are offline", QIcon{":/icons/graypower.png"}};
	s_runningNotConnected = {"You are offline", QIcon{":/icons/graylight.png"}};

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
	QString projectId;
	QString description;
	auto entries = m_user->getTimeEntries();

	if (!m_useLastProject)
		projectId = m_defaultProjectId;
	else
	{
		bool projectIdLoaded{false};

		for (const auto &entry : entries)
		{
			if (entry.project().id().isEmpty()) [[unlikely]]
			{
				std::cerr << "getting project id failed\n";
				continue; // no project id to see here, move along
			}
			else if (entry.project().id() != TIME_301)
			{
				projectId = entry.project().id();
				projectIdLoaded = true;
				break;
			}
		}

		// when all else fails, use the first extant project
		if (!projectIdLoaded) [[unlikely]]
			projectId = ClockifyManager::instance()->projects().first().id();
	}

	if (m_disableDescription)
		; // description is already empty, so we will fall through
	else if (!m_useLastDescription)
		description = m_defaultDescription;
	else
	{
		bool descriptionLoaded{false};

		for (const auto &entry : entries)
		{
			if (entry.project().id() != TIME_301)
			{
				description = entry.project().description();
				descriptionLoaded = true;
				break;
			}
		}

		if (!descriptionLoaded) [[unlikely]]
			description = ClockifyManager::instance()->projects().first().description();
	}

	return ClockifyProject{projectId, ClockifyManager::instance()->projectName(projectId), description};
}

void TrayIcons::setUser(QSharedPointer<ClockifyUser> user)
{
	m_user.clear();
	m_user = user;
}

void TrayIcons::updateTrayIcons()
{

	if (!ClockifyManager::instance()->isConnectedToInternet()) [[unlikely]]
	{
		setClockifyRunningIconTooltip(s_powerNotConnected);
		setRunningJobIconTooltip(s_runningNotConnected);
	}
	else if (m_user->hasRunningTimeEntry())
	{
		setClockifyRunningIconTooltip(s_clockifyOn);

		try {
			if (m_user->getRunningTimeEntry().project().id() == TIME_301)
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
	for (auto &project : ClockifyManager::instance()->projects())
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

	ClockifyManager::instance()->setApiKey(m_apiKey);

	QSettings settings;
	settings.setValue("apiKey", m_apiKey);
}

void TrayIcons::showAboutDialog()
{
	// put this into a variable to handle this gonzo string more nicely
	constexpr auto licenseInfo = "ClockifyTrayIcons copyright © 2020. Licensed " \
								 "under the MIT license with Christian Light Internal Software exceptions.\n\n" \
								 "Icons from [1RadicalOne](https://commons.wikimedia.org/wiki/User:1RadicalOne) " \
								 "(light icons, licensed [CC BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)) and " \
								 "[Microsoft](https://github.com/microsoft/fluentui-system-icons) " \
								 "(power icon, licensed [MIT](https://mit-license.org)).";

	auto showLicense = [this](QWidget *parent = nullptr) {
		QFile licenseFile{":/LICENSE"};
		QString licenseText;
		if (licenseFile.open(QIODevice::ReadOnly))
			licenseText = licenseFile.readAll();
		else
			licenseText = "Error: could not load the license. Please contact the Christian Light IT departement for a copy of the license.";

		QDialog dialog{parent};

		auto layout = new QVBoxLayout{&dialog};

		auto licenseView = new QTextBrowser{&dialog};
		licenseView->setText(licenseText);
		licenseView->setOpenExternalLinks(true);
		licenseView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
		layout->addWidget(licenseView);

		auto bb = new QDialogButtonBox{QDialogButtonBox::Ok, &dialog};
		connect(bb, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

		layout->addWidget(bb);

		dialog.setLayout(layout);
		dialog.setWindowTitle("MIT license with Christian Light Internal Software exceptions");
		dialog.setModal(true);
		dialog.resize(600, 600);
		dialog.move(dialog.screen()->geometry().width() / 2 - dialog.width() / 2, dialog.screen()->geometry().height() / 2 - dialog.height() / 2);
		dialog.exec();
	};

	QDialog dialog;

	auto layout = new QVBoxLayout{&dialog};

	auto infoLabel = new QLabel{licenseInfo, &dialog};
	infoLabel->setTextFormat(Qt::MarkdownText);
	infoLabel->setWordWrap(true);
	layout->addWidget(infoLabel);

	auto bb = new QDialogButtonBox{QDialogButtonBox::Ok, &dialog};
	connect(bb->addButton("Show license", QDialogButtonBox::ActionRole), &QPushButton::clicked, this, [&dialog, &showLicense] {
		showLicense(&dialog);
	});
	connect(bb, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

	layout->addWidget(bb);

	dialog.setLayout(layout);
	dialog.setWindowTitle("About " + qApp->applicationName());
	dialog.resize(500, dialog.heightForWidth(500));
	dialog.move(dialog.screen()->geometry().width() / 2 - dialog.width() / 2, dialog.screen()->geometry().height() / 2 - dialog.height() / 2);
	dialog.exec();
}

void TrayIcons::setUpTrayIcons()
{
	auto m_clockifyRunningMenu = new QMenu;
	auto m_runningJobMenu = new QMenu;

	// set up the menu actions
	connect(m_clockifyRunningMenu->addAction("Start"), &QAction::triggered, this, [this]() {
		if (ClockifyManager::instance()->isConnectedToInternet() == false) [[unlikely]]
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");

		if (!m_user->hasRunningTimeEntry()) [[likely]]
		{
			auto project = defaultProject();
			m_user->startTimeEntry(project.id(), project.description());
			updateTrayIcons();
		}
	});
	connect(m_clockifyRunningMenu->addAction("Stop"), &QAction::triggered, this, [this]() {
		if (ClockifyManager::instance()->isConnectedToInternet() == false) [[unlikely]]
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");

		if (m_user->hasRunningTimeEntry()) [[likely]]
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
	connect(m_clockifyRunningMenu->addAction("About Qt"), &QAction::triggered, this, []() { QMessageBox::aboutQt(nullptr); });
	connect(m_clockifyRunningMenu->addAction("About"), &QAction::triggered, this, &TrayIcons::showAboutDialog);
	connect(m_clockifyRunningMenu->addAction("Quit"), &QAction::triggered, qApp, &QApplication::quit);

	connect(m_runningJobMenu->addAction("Break"), &QAction::triggered, this, [this]() {
		if (ClockifyManager::instance()->isConnectedToInternet() == false) [[unlikely]]
		{
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");
			return;
		}

		QDateTime start = QDateTime::currentDateTimeUtc();

		if (m_user->hasRunningTimeEntry())
		{
			try
			{
				if (m_user->getRunningTimeEntry().project().id() == TIME_301)
					return;
			}
			catch (const std::exception &) {}

			start = m_user->stopCurrentTimeEntry();
		}
		m_user->startTimeEntry(TIME_301, start);
		updateTrayIcons();
	});
	connect(m_runningJobMenu->addAction("Resume work"), &QAction::triggered, this, [this]() {
		if (ClockifyManager::instance()->isConnectedToInternet() == false) [[unlikely]]
		{
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");
			return;
		}

		QDateTime start = QDateTime::currentDateTimeUtc();

		if (m_user->hasRunningTimeEntry())
		{
			try
			{
				if (m_user->getRunningTimeEntry().project().id() != TIME_301)
					return;
			}
			catch (const std::exception &) {}

			start = m_user->stopCurrentTimeEntry();
		}
		auto project = defaultProject();
		m_user->startTimeEntry(project.id(), project.description(), start);
		updateTrayIcons();
	});
	connect(m_runningJobMenu->addAction("Change default project"), &QAction::triggered, this, &TrayIcons::getNewProjectId);
	connect(m_runningJobMenu->addAction("Change API key"), &QAction::triggered, this, &TrayIcons::getNewApiKey);
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

		if (ClockifyManager::instance()->isConnectedToInternet() == false) [[unlikely]]
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
	connect(m_runningJob, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
		m_eventLoop.stop();

		if (reason != QSystemTrayIcon::Trigger && reason != QSystemTrayIcon::DoubleClick) [[unlikely]]
		{
			m_eventLoop.start();
			return;
		}

		if (ClockifyManager::instance()->isConnectedToInternet() == false) [[unlikely]]
		{
			m_clockifyRunning->showMessage("Internet connection lost", "The request could not be completed because the internet connection is down.");
			m_eventLoop.start();
			return;
		}

		if (m_user->hasRunningTimeEntry())
		{
			if (m_user->getRunningTimeEntry().project().id() == TIME_301)
			{
				auto time = m_user->stopCurrentTimeEntry();
				auto project = defaultProject();
				m_user->startTimeEntry(project.id(), project.description(), time);
			}
			else
			{
				auto time = m_user->stopCurrentTimeEntry();
				m_user->startTimeEntry(TIME_301, time);
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
