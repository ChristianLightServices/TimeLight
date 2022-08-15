#include "SettingsDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QShortcut>
#include <QTabWidget>
#include <QVBoxLayout>

#include "Logger.h"
#include "Settings.h"
#include "TeamsClient.h"
#include "Utils.h"

namespace logs = TimeLight::logs;

SettingsDialog::SettingsDialog(AbstractTimeServiceManager *manager,
                               const QList<QPair<QString, QString>> &availableManagers,
                               QWidget *parent)
    : QDialog{parent},
      m_manager{manager},
      m_availableManagers{availableManagers},
      m_tabWidget{new QTabWidget}
{
    for (auto &project : m_manager->projects())
    {
        m_availableProjects.first.push_back(project.id());
        m_availableProjects.second.push_back(project.name());
    }

    auto buttons = new QDialogButtonBox{this};
    buttons->addButton(tr("Done"), QDialogButtonBox::AcceptRole);

    m_backendPage = createBackendPage();
    m_projectPage = createProjectPage();
    m_appPage = createAppPage();
    m_teamsPage = createTeamsPage();
    m_tabWidget->addTab(m_backendPage, tr("Backend settings"));
    m_tabWidget->addTab(m_projectPage, tr("Default project"));
    m_tabWidget->addTab(m_appPage, tr("App settings"));
    m_tabWidget->addTab(m_teamsPage, tr("Microsoft Teams"));

    auto mainWidgetLayout = new QVBoxLayout{this};
    mainWidgetLayout->addWidget(m_tabWidget);
    mainWidgetLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    if (!Settings::instance()->developerMode())
    {
        auto devMode = new QShortcut{Qt::Key_F7, this};
        connect(devMode, &QShortcut::activated, this, [this] {
            QMessageBox::information(this, tr("Developer mode"), tr("Developer mode has been enabled!"), QMessageBox::Ok);
            logs::app()->trace("Developer mode enabled");
            Settings::instance()->setDeveloperMode(true);
        });
    }

    resize(600, -1);
}

void SettingsDialog::switchToPage(Pages page)
{
    switch (page)
    {
    case Pages::BackendPage:
        m_tabWidget->setCurrentWidget(m_backendPage);
        break;
    case Pages::ProjectPage:
        m_tabWidget->setCurrentWidget(m_projectPage);
        break;
    case Pages::AppPage:
        m_tabWidget->setCurrentWidget(m_appPage);
        break;
    case Pages::TeamsPage:
        m_tabWidget->setCurrentWidget(m_teamsPage);
        break;
    }
}

QWidget *SettingsDialog::createBackendPage()
{
    auto backendPage{new QWidget};
    auto layout{new QGridLayout{backendPage}};

    auto timeServices{new QComboBox{backendPage}};
    for (const auto &manager : m_availableManagers)
        timeServices->addItem(manager.first, manager.second);
    for (int i = 0; i < m_availableManagers.size(); ++i)
    {
        if (m_availableManagers[i].second == Settings::instance()->timeService())
        {
            timeServices->setCurrentIndex(i);
            break;
        }
    }

    layout->addWidget(new QLabel{tr("Time service to use"), backendPage}, 0, 0);
    layout->addWidget(timeServices, 0, 1, 1, 2);

    auto apiKeyInput{new QLineEdit{Settings::instance()->apiKey(), backendPage}};
    apiKeyInput->setCursorPosition(0);
    auto useNewApiKey{new QPushButton{tr("Use"), backendPage}};
    useNewApiKey->setDisabled(true);

    layout->addWidget(new QLabel{tr("API key"), backendPage}, 1, 0);
    layout->addWidget(apiKeyInput, 1, 1);
    layout->addWidget(useNewApiKey, 1, 2);

    auto workspaces{new QComboBox{backendPage}};
    for (const auto &workspace : m_manager->workspaces())
        workspaces->addItem(workspace.name(), workspace.id());
    workspaces->setCurrentIndex(m_manager->workspaces().indexOf(m_manager->currentWorkspace()));

    layout->addWidget(new QLabel{tr("Workspace to track time on"), backendPage}, 2, 0);
    layout->addWidget(workspaces, 2, 1, 1, 2);

    TimeLight::addVerticalStretchToQGridLayout(layout);

    connect(timeServices, &QComboBox::currentIndexChanged, timeServices, [this, timeServices](int i) {
        if (m_resetOfTimeServiceComboBoxInProgress)
        {
            m_resetOfTimeServiceComboBoxInProgress = false;
            return;
        }

        if (m_availableManagers[i].second == QStringLiteral("com.timecamp") &&
            Settings::instance()->eventLoopInterval() < 15000)
            if (QMessageBox::question(this,
                                      tr("Increase update interval"),
                                      tr("TimeCamp only allows apps to check for updates 360 times per hour. Do you want to "
                                         "increase the update interval to 15 seconds to ensure that you do not exceed this "
                                         "update limit?")))
                Settings::instance()->setEventLoopInterval(15000);

        switch (QMessageBox::information(this,
                                         tr("Restart app to continue"),
                                         tr("To continue, the app will need to restart."),
                                         QMessageBox::Ok | QMessageBox::Cancel,
                                         QMessageBox::Ok))
        {
        case QMessageBox::Ok:
            Settings::instance()->setTimeService(timeServices->currentData().toString());
            logs::app()->trace("Setting time service to {}", Settings::instance()->timeService().toStdString());
            TimeLight::restartApp();
            break;
        case QMessageBox::Cancel:
        default:
            m_resetOfTimeServiceComboBoxInProgress = true;
            timeServices->setCurrentIndex(
                m_availableManagers.indexOf({m_manager->serviceName(), m_manager->serviceIdentifier()}));
            break;
        }
    });

    connect(apiKeyInput, &QLineEdit::textChanged, this, [apiKeyInput, useNewApiKey] {
        useNewApiKey->setEnabled(Settings::instance()->apiKey() != apiKeyInput->text());
    });
    connect(useNewApiKey, &QPushButton::clicked, this, [this, apiKeyInput] {
        if (QMessageBox::warning(this,
                                 tr("Restart required"),
                                 tr("To continue changing the API key, the program will restart."),
                                 QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok)
        {
            m_manager->setApiKey(apiKeyInput->text());
            Settings::instance()->setApiKey(apiKeyInput->text());
            logs::app()->trace("Changing API key");
            TimeLight::restartApp();
        }
    });

    connect(workspaces, &QComboBox::currentIndexChanged, workspaces, [this, workspaces] {
        Settings::instance()->setWorkspaceId(workspaces->currentData().toString());
        logs::app()->trace("Changing default workspace");
        m_manager->setWorkspaceId(workspaces->currentData().toString());
    });

    return backendPage;
}

QWidget *SettingsDialog::createProjectPage()
{
    auto projectPage{new QWidget};
    auto layout{new QVBoxLayout{projectPage}};

    auto projectGroup{new QGroupBox{tr("Default project settings"), projectPage}};
    auto breakProjectGroup{new QGroupBox{tr("Break project settings"), projectPage}};
    auto descriptionGroup{new QGroupBox{tr("Default description settings"), projectPage}};

    auto projectGroupLayout{new QGridLayout{projectGroup}};

    auto useLastProjectBtn{new QRadioButton{tr("Use the project from the last task"), projectPage}};
    auto useSpecificProjectBtn{new QRadioButton{tr("Use a specific project every time"), projectPage}};
    auto defaultProjectCombo{new QComboBox{projectPage}};

    auto projectButtons{new QButtonGroup};
    projectButtons->addButton(useLastProjectBtn);
    projectButtons->addButton(useSpecificProjectBtn);

    if (Settings::instance()->useLastProject())
    {
        useLastProjectBtn->setChecked(true);
        defaultProjectCombo->setEnabled(false);
    }
    else
        useSpecificProjectBtn->setChecked(true);

    defaultProjectCombo->addItems(m_availableProjects.second);
    defaultProjectCombo->setCurrentIndex(m_availableProjects.first.indexOf(Settings::instance()->projectId().isEmpty() ?
                                                                               m_availableProjects.first.first() :
                                                                               Settings::instance()->projectId()));

    projectGroupLayout->addWidget(useSpecificProjectBtn, 0, 0);
    projectGroupLayout->addWidget(defaultProjectCombo, 0, 1);
    projectGroupLayout->addWidget(useLastProjectBtn, 1, 0);

    auto breakProjectGroupLayout{new QGridLayout{breakProjectGroup}};

    auto useBreakTime{new QCheckBox{tr("Use a separate break time project"), breakProjectGroup}};
    useBreakTime->setChecked(Settings::instance()->useSeparateBreakTime());

    auto breakProject{new QComboBox{breakProjectGroup}};
    breakProject->setEnabled(useBreakTime->isChecked());
    breakProject->addItems(m_availableProjects.second);
    breakProject->setCurrentIndex(m_availableProjects.first.indexOf(Settings::instance()->breakTimeId().isEmpty() ?
                                                                        m_availableProjects.second.first() :
                                                                        Settings::instance()->breakTimeId()));

    breakProjectGroupLayout->addWidget(useBreakTime, 0, 0);
    breakProjectGroupLayout->addWidget(breakProject, 0, 1);

    auto descriptionGroupLayout{new QGridLayout{descriptionGroup}};

    auto useLastDescriptionBtn{new QRadioButton{tr("Use the description from the last task"), projectPage}};
    auto useSpecificDescriptionBtn{new QRadioButton{tr("Use a specific description every time"), projectPage}};
    auto useNoDescriptionBtn{new QRadioButton{tr("Don't use a description"), projectPage}};

    auto descriptionButtons{new QButtonGroup{projectPage}};
    descriptionButtons->addButton(useLastDescriptionBtn);
    descriptionButtons->addButton(useSpecificDescriptionBtn);
    descriptionButtons->addButton(useNoDescriptionBtn);

    auto defaultDescriptionEdit{new QLineEdit{Settings::instance()->description(), projectPage}};

    if (Settings::instance()->disableDescription())
    {
        useNoDescriptionBtn->setChecked(true);
        defaultDescriptionEdit->setEnabled(false);
    }
    else if (Settings::instance()->useLastDescription())
    {
        useLastDescriptionBtn->setChecked(true);
        defaultDescriptionEdit->setEnabled(false);
    }
    else
        useSpecificDescriptionBtn->setChecked(true);

    descriptionGroupLayout->addWidget(useSpecificDescriptionBtn, 0, 0);
    descriptionGroupLayout->addWidget(defaultDescriptionEdit, 0, 1);
    descriptionGroupLayout->addWidget(useLastDescriptionBtn, 1, 0);
    descriptionGroupLayout->addWidget(useNoDescriptionBtn, 2, 0);

    descriptionGroup->setLayout(descriptionGroupLayout);

    layout->addWidget(projectGroup);
    layout->addWidget(breakProjectGroup);
    layout->addWidget(descriptionGroup);

    layout->addStretch();

    connect(projectButtons,
            QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
            projectButtons,
            [projectButtons, useSpecificProjectBtn, defaultProjectCombo, useLastProjectBtn](QAbstractButton *) {
                if (projectButtons->checkedButton() == static_cast<QAbstractButton *>(useSpecificProjectBtn))
                {
                    Settings::instance()->setUseLastProject(false);
                    defaultProjectCombo->setEnabled(true);
                }
                else
                {
                    if (projectButtons->checkedButton() == static_cast<QAbstractButton *>(useLastProjectBtn))
                        Settings::instance()->setUseLastProject(true);

                    defaultProjectCombo->setEnabled(false);
                }
            });

    connect(defaultProjectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), defaultProjectCombo, [this](int i) {
        Settings::instance()->setProjectId(m_availableProjects.first[i]);
        logs::app()->trace("Changed default project");
    });

    connect(useBreakTime, &QCheckBox::stateChanged, Settings::instance(), &Settings::setUseSeparateBreakTime);
    connect(useBreakTime, &QCheckBox::stateChanged, breakProject, &QWidget::setEnabled);

    connect(breakProject, &QComboBox::currentIndexChanged, breakProject, [this, breakProject](int i) {
        if (m_availableProjects.first[i] == Settings::instance()->breakTimeId())
        {
            QMessageBox::warning(
                this, tr("Invalid choice"), tr("You cannot set the same default project and break project!"));
            breakProject->setCurrentIndex(m_availableProjects.first.indexOf(Settings::instance()->breakTimeId()));
            logs::app()->trace("Resetting default project to avoid overriding break project");
        }
        else
        {
            Settings::instance()->setBreakTimeId(m_availableProjects.first[i]);
            logs::app()->trace("Changed break project");
        }
    });

    connect(
        descriptionButtons,
        QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
        descriptionButtons,
        [descriptionButtons, useSpecificDescriptionBtn, defaultDescriptionEdit, useLastDescriptionBtn, useNoDescriptionBtn](
            QAbstractButton *) {
            if (descriptionButtons->checkedButton() == static_cast<QAbstractButton *>(useSpecificDescriptionBtn))
            {
                Settings::instance()->setUseLastDescription(false);
                Settings::instance()->setDisableDescription(false);
                defaultDescriptionEdit->setEnabled(true);
            }
            else
            {
                if (descriptionButtons->checkedButton() == static_cast<QAbstractButton *>(useLastDescriptionBtn))
                {
                    Settings::instance()->setUseLastDescription(true);
                    Settings::instance()->setDisableDescription(false);
                }
                else if (descriptionButtons->checkedButton() == static_cast<QAbstractButton *>(useNoDescriptionBtn))
                {
                    Settings::instance()->setUseLastDescription(false);
                    Settings::instance()->setDisableDescription(true);
                }

                defaultDescriptionEdit->setEnabled(false);
            }
        });

    connect(defaultDescriptionEdit, &QLineEdit::textChanged, defaultDescriptionEdit, [defaultDescriptionEdit]() {
        Settings::instance()->setDescription(defaultDescriptionEdit->text());
    });

    return projectPage;
}

QWidget *SettingsDialog::createAppPage()
{
    auto appPage{new QWidget};
    auto layout{new QGridLayout{appPage}};

    auto preventSplittingEntries{new QCheckBox{tr("Prevent consecutive entries with the same project"), appPage}};
    preventSplittingEntries->setToolTip(tr("If enabled, TimeLight will not start a new time entry if there is a currently "
                                           "running entry with the same project."));
    preventSplittingEntries->setChecked(Settings::instance()->preventSplittingEntries());

    auto middleClickForBreak{new QCheckBox{tr("Click with the middle mouse button to switch to break time"), appPage}};
    middleClickForBreak->setToolTip(tr("If enabled, TimeLight will display only the primary button. Click the button with "
                                       "the scroll wheel to access break time."));
    middleClickForBreak->setChecked(Settings::instance()->middleClickForBreak());
    middleClickForBreak->setEnabled(Settings::instance()->useSeparateBreakTime());

    auto eventLoopIntervalLabel =
        new QLabel{tr("Interval between updates of %1 data").arg(m_manager->serviceName()), appPage};
    eventLoopIntervalLabel->setToolTip(tr("After TimeLight fetches data from %1, it will wait for this many seconds before "
                                          "fetching updated data. For some time services, you need to set this to at least "
                                          "15 or 30 seconds or TimeLight will be blocked.")
                                           .arg(m_manager->serviceName()));
    auto eventLoopInterval{new QDoubleSpinBox{appPage}};
    eventLoopInterval->setSuffix(tr(" seconds"));
    eventLoopInterval->setSingleStep(0.5);
    eventLoopInterval->setValue(static_cast<double>(Settings::instance()->eventLoopInterval()) / 1000);
    eventLoopInterval->setDecimals(1);
    eventLoopInterval->setMinimum(0);
    eventLoopInterval->setMaximum(30);

    auto showNotifications{new QCheckBox{tr("Show how much time was worked when a job is stopped"), appPage}};
    showNotifications->setChecked(Settings::instance()->showDurationNotifications());

    auto showTimeUpWarning{new QCheckBox{tr("Notify when a full week's time has been logged"), appPage}};
    showTimeUpWarning->setChecked(Settings::instance()->alertOnTimeUp());

    auto weekHours{new QDoubleSpinBox{appPage}};
    weekHours->setSuffix(tr(" hours"));
    weekHours->setSingleStep(0.5);
    weekHours->setValue(Settings::instance()->weekHours());
    weekHours->setDecimals(1);
    weekHours->setMinimum(1);
    weekHours->setMaximum(100);
    weekHours->setEnabled(Settings::instance()->alertOnTimeUp());

    layout->addWidget(preventSplittingEntries, 0, 0);
    layout->addWidget(middleClickForBreak, 1, 0);
    layout->addWidget(eventLoopIntervalLabel, 2, 0);
    layout->addWidget(eventLoopInterval, 2, 1);
    layout->addWidget(showNotifications, 3, 0, 1, 2);
    layout->addWidget(showTimeUpWarning, 4, 0, 1, 2);
    layout->addWidget(new QLabel{tr("Duration of a work week"), appPage}, 5, 0);
    layout->addWidget(weekHours, 5, 1);

    TimeLight::addVerticalStretchToQGridLayout(layout);

    connect(preventSplittingEntries, &QCheckBox::stateChanged, Settings::instance(), &Settings::setPreventSplittingEntries);
    connect(Settings::instance(), &Settings::useSeparateBreakTimeChanged, middleClickForBreak, [middleClickForBreak] {
        middleClickForBreak->setEnabled(Settings::instance()->useSeparateBreakTime());
    });
    connect(middleClickForBreak, &QCheckBox::stateChanged, Settings::instance(), &Settings::setMiddleClickForBreak);
    connect(eventLoopInterval, &QDoubleSpinBox::valueChanged, eventLoopInterval, [](double d) {
        Settings::instance()->setEventLoopInterval(static_cast<int>(d * 1000));
    });
    connect(showNotifications, &QCheckBox::stateChanged, Settings::instance(), &Settings::setShowDurationNotifications);
    connect(showTimeUpWarning, &QCheckBox::stateChanged, Settings::instance(), &Settings::setAlertOnTimeUp);
    connect(weekHours, &QDoubleSpinBox::valueChanged, Settings::instance(), &Settings::setWeekHours);

    return appPage;
}

QWidget *SettingsDialog::createTeamsPage()
{
    auto teamsPage{new QWidget{this}};
    auto layout{new QGridLayout{teamsPage}};

    auto useTeams = new QCheckBox{tr("Set presence in Microsoft Teams to match work status"), teamsPage};
    useTeams->setToolTip(tr("If enabled, TimeLight will attempt to your Teams presence based on whether or not you are "
                            "working. You will need to clear your presence in Teams to make this work."));
    useTeams->setChecked(Settings::instance()->useTeamsIntegration());

    auto presenceWhenWorking = new QComboBox{teamsPage};
    auto presenceWhenOnBreak = new QComboBox{teamsPage};
    auto presenceWhenNotWorking = new QComboBox{teamsPage};
    for (auto &combo : {presenceWhenWorking, presenceWhenOnBreak, presenceWhenNotWorking})
    {
        combo->addItem(tr("Available"), QVariant::fromValue(TeamsClient::Presence::Available));
        combo->addItem(tr("In a call"), QVariant::fromValue(TeamsClient::Presence::InACall));
        combo->addItem(tr("In a conference call"), QVariant::fromValue(TeamsClient::Presence::InAConferenceCall));
        combo->addItem(tr("Away"), QVariant::fromValue(TeamsClient::Presence::Away));

        combo->setEnabled(Settings::instance()->useTeamsIntegration());
        connect(Settings::instance(), &Settings::useTeamsIntegrationChanged, combo, [combo] {
            combo->setEnabled(Settings::instance()->useTeamsIntegration());
        });
    }
    presenceWhenWorking->setCurrentIndex(static_cast<int>(Settings::instance()->presenceWhileWorking()));
    presenceWhenOnBreak->setCurrentIndex(static_cast<int>(Settings::instance()->presenceWhileOnBreak()));
    presenceWhenNotWorking->setCurrentIndex(static_cast<int>(Settings::instance()->presenceWhileNotWorking()));

    auto breakLabel = new QLabel{tr("Teams presence while on break"), teamsPage};
    presenceWhenOnBreak->setVisible(Settings::instance()->useSeparateBreakTime());
    breakLabel->setVisible(Settings::instance()->useSeparateBreakTime());
    connect(Settings::instance(),
            &Settings::useSeparateBreakTimeChanged,
            presenceWhenOnBreak,
            [presenceWhenOnBreak, breakLabel] {
                presenceWhenOnBreak->setVisible(Settings::instance()->useSeparateBreakTime());
                breakLabel->setVisible(Settings::instance()->useSeparateBreakTime());
            });

    layout->addWidget(useTeams, 0, 0, 1, 2);
    layout->addWidget(new QLabel{tr("Teams presence while working"), teamsPage}, 1, 0);
    layout->addWidget(breakLabel, 2, 0);
    layout->addWidget(new QLabel{tr("Teams presence while not working"), teamsPage}, 3, 0);
    layout->addWidget(presenceWhenWorking, 1, 1);
    layout->addWidget(presenceWhenOnBreak, 2, 1);
    layout->addWidget(presenceWhenNotWorking, 3, 1);

    TimeLight::addVerticalStretchToQGridLayout(layout);

    connect(useTeams, &QCheckBox::clicked, this, [](bool state) {
        Settings::instance()->setUseTeamsIntegration(state);
        logs::teams()->trace("Teams integration enabled");
    });
    connect(presenceWhenWorking, &QComboBox::currentIndexChanged, this, [presenceWhenWorking] {
        Settings::instance()->setPresenceWhileWorking(presenceWhenWorking->currentData().value<TeamsClient::Presence>());
    });
    connect(presenceWhenOnBreak, &QComboBox::currentIndexChanged, this, [presenceWhenOnBreak] {
        Settings::instance()->setPresenceWhileOnBreak(presenceWhenOnBreak->currentData().value<TeamsClient::Presence>());
    });
    connect(presenceWhenNotWorking, &QComboBox::currentIndexChanged, this, [presenceWhenNotWorking] {
        Settings::instance()->setPresenceWhileNotWorking(
            presenceWhenNotWorking->currentData().value<TeamsClient::Presence>());
    });

    return teamsPage;
}
