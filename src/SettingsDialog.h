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
                            const QList<QPair<QString, QString>> &availableManagers,
                            QWidget *parent = nullptr);

    enum class Pages
    {
        BackendPage,
        ProjectPage,
        AppPage,
        TeamsPage,
    };

    void switchToPage(Pages page);

private:
    QWidget *createBackendPage();
    QWidget *createProjectPage();
    QWidget *createAppPage();
    QWidget *createTeamsPage();

    AbstractTimeServiceManager *m_manager;
    // ordered as {name, id}
    QList<QPair<QString, QString>> m_availableManagers;

    QTabWidget *m_tabWidget;
    QWidget *m_backendPage;
    QWidget *m_projectPage;
    QWidget *m_appPage;
    QWidget *m_teamsPage;

    QPair<QStringList, QStringList> m_availableProjects;
};

#endif // SETTINGSDIALOG_H
