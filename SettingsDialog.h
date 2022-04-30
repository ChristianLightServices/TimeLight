#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>

#include "AbstractTimeServiceManager.h"

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(AbstractTimeServiceManager *manager,
                            const QList<AbstractTimeServiceManager *> &availableManagers,
                            QWidget *parent = nullptr);

    enum class Pages
    {
        BackendPage,
        ProjectPage,
        AppPage,
    };

    void switchToPage(Pages page);

private:
    QWidget *createBackendPage();
    QWidget *createProjectPage();
    QWidget *createAppPage();

    AbstractTimeServiceManager *m_manager;
    QList<AbstractTimeServiceManager *> m_availableManagers;

    QTabWidget *m_tabWidget;
    QWidget *m_backendPage;
    QWidget *m_projectPage;
    QWidget *m_appPage;

    QPair<QStringList, QStringList> m_availableProjects;

    // Unfortunately, these are necessary to make sure that certain dialog boxes are only shown once.
    bool m_resetOfTimeServiceComboBoxInProgress{false};
    bool m_resetOfApiKeyInProgress{false};
};

#endif // SETTINGSDIALOG_H
