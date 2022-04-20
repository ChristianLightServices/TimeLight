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
#include <QTabWidget>
#include <QVBoxLayout>

#include "Settings.h"
#include "User.h"
#include "Utils.h"

SettingsDialog::SettingsDialog(AbstractTimeServiceManager *manager,
                               const QList<AbstractTimeServiceManager *> &availableManagers,
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

    auto buttons = new QDialogButtonBox{QDialogButtonBox::Ok};

    m_backendPage = createBackendPage();
    m_projectPage = createProjectPage();
    m_appPage = createAppPage();
    m_tabWidget->addTab(m_backendPage, tr("Backend settings"));
    m_tabWidget->addTab(m_projectPage, tr("Default project"));
    m_tabWidget->addTab(m_appPage, tr("App settings"));

    auto mainWidgetLayout = new QVBoxLayout{this};
    mainWidgetLayout->addWidget(m_tabWidget);
    mainWidgetLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

void SettingsDialog::switchToPage(Pages page)
{
    switch (page)
    {
    case Pages::BackendPage:
        m_tabWidget->setCornerWidget(m_backendPage);
        break;
    case Pages::ProjectPage:
        m_tabWidget->setCurrentWidget(m_projectPage);
        break;
    case Pages::AppPage:
        m_tabWidget->setCurrentWidget(m_appPage);
        break;
    }
}

QWidget *SettingsDialog::createBackendPage()
{
    auto backendPage{new QWidget};
    auto layout{new QGridLayout{backendPage}};

    auto timeServices{new QComboBox{backendPage}};
    for (const auto &manager : m_availableManagers)
        timeServices->addItem(manager->serviceName(), manager->serviceIdentifier());

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
    for (const auto &workspace : m_manager->getOwnerWorkspaces())
        workspaces->addItem(workspace.name(), workspace.id());

    layout->addWidget(new QLabel{tr("Workspace to track time on"), backendPage}, 2, 0);
    layout->addWidget(workspaces, 2, 1, 1, 2);

    ClockifyTrayIcons::addVerticalStretchToQGridLayout(layout);

    connect(timeServices, &QComboBox::currentIndexChanged, timeServices, [this, timeServices](int i) {
        if (m_resetOfTimeServiceComboBoxInProgress)
        {
            m_resetOfTimeServiceComboBoxInProgress = false;
            return;
        }

        switch (QMessageBox::information(this,
                                         tr("Restart app to continue"),
                                         tr("To continue, the app will need to restart."),
                                         QMessageBox::Ok | QMessageBox::Cancel,
                                         QMessageBox::Ok))
        {
        case QMessageBox::Ok:
            Settings::instance()->setTimeService(m_availableManagers[i]->serviceIdentifier());
            ClockifyTrayIcons::restartApp();
            break;
        case QMessageBox::Cancel:
        default:
            m_resetOfTimeServiceComboBoxInProgress = true;
            timeServices->setCurrentIndex(m_availableManagers.indexOf(m_manager));
            break;
        }
    });

    connect(apiKeyInput, &QLineEdit::textChanged, this, [apiKeyInput, useNewApiKey] {
        useNewApiKey->setEnabled(Settings::instance()->apiKey() != apiKeyInput->text());
    });
    connect(useNewApiKey, &QPushButton::clicked, this, [this, apiKeyInput](int) {
        if (QMessageBox::warning(this,
                                 tr("Restart required"),
                                 tr("To continue changing the API key, the program will restart."),
                                 QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok)
        {
            m_manager->setApiKey(apiKeyInput->text());
            Settings::instance()->setApiKey(apiKeyInput->text());
            ClockifyTrayIcons::restartApp();
        }
    });

    connect(workspaces, &QComboBox::currentIndexChanged, workspaces, [workspaces](int) {
        Settings::instance()->setWorkspaceId(workspaces->currentData().toString());
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
    });

    connect(useBreakTime, &QCheckBox::stateChanged, breakProject, [this, useBreakTime, breakProject](int state) {
        if (QMessageBox::information(this,
                                     tr("Restart required"),
                                     tr("To continue, the program will restart"),
                                     QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok)
        {
            switch (state)
            {
            case Qt::Checked:
            case Qt::PartiallyChecked:
                Settings::instance()->setUseSeparateBreakTime(true);
                breakProject->setEnabled(true);
                break;
            case Qt::Unchecked:
                Settings::instance()->setUseSeparateBreakTime(false);
                breakProject->setDisabled(true);
                break;
            default:
                break;
            }
            ClockifyTrayIcons::restartApp();
        }
        else
            useBreakTime->setChecked(Settings::instance()->useSeparateBreakTime());
    });

    connect(breakProject, &QComboBox::currentIndexChanged, breakProject, [this, breakProject](int i) {
        if (m_availableProjects.first[i] == Settings::instance()->breakTimeId())
        {
            QMessageBox::warning(
                this, tr("Invalid choice"), tr("You cannot set the same default project and break project!"));
            breakProject->setCurrentIndex(m_availableProjects.first.indexOf(Settings::instance()->breakTimeId()));
        }
        else
            Settings::instance()->setBreakTimeId(m_availableProjects.first[i]);
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

    auto eventLoopInterval{new QDoubleSpinBox{appPage}};
    eventLoopInterval->setSuffix(tr(" seconds"));
    eventLoopInterval->setSingleStep(0.5);
    eventLoopInterval->setValue(Settings::instance()->eventLoopInterval() / 1000);
    eventLoopInterval->setDecimals(1);
    eventLoopInterval->setMinimum(0);
    eventLoopInterval->setMaximum(10);

    auto showNotifications{new QCheckBox{tr("Show how much time was worked when a job is stopped"), appPage}};
    showNotifications->setChecked(Settings::instance()->showDurationNotifications());

    layout->addWidget(new QLabel{tr("Interval between updates of %1 data").arg(m_manager->serviceName()), appPage}, 0, 0);
    layout->addWidget(eventLoopInterval, 0, 1);
    layout->addWidget(showNotifications, 1, 0, 1, 2);

    ClockifyTrayIcons::addVerticalStretchToQGridLayout(layout);

    connect(eventLoopInterval, &QDoubleSpinBox::valueChanged, eventLoopInterval, [](double d) {
        Settings::instance()->setEventLoopInterval(static_cast<int>(d) * 1000);
    });
    connect(showNotifications, &QCheckBox::stateChanged, showNotifications, [](int state) {
        switch (state)
        {
        case Qt::Checked:
        case Qt::PartiallyChecked:
            Settings::instance()->setShowDurationNotifications(true);
            break;
        case Qt::Unchecked:
            Settings::instance()->setShowDurationNotifications(false);
            break;
        default:
            break;
        }
    });

    return appPage;
}
