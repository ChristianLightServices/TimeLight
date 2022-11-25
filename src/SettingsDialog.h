#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>

#include "AbstractTimeServiceManager.h"

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QSharedPointer<AbstractTimeServiceManager> manager,
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
    QSharedPointer<QWidget> createBackendPage();
    QSharedPointer<QWidget> createProjectPage();
    QSharedPointer<QWidget> createAppPage();
    QSharedPointer<QWidget> createTeamsPage();

    QSharedPointer<AbstractTimeServiceManager> m_manager;
    // ordered as {name, id}
    QList<QPair<QString, QString>> m_availableManagers;

    QTabWidget *m_tabWidget;
    QSharedPointer<QWidget> m_backendPage;
    QSharedPointer<QWidget> m_projectPage;
    QSharedPointer<QWidget> m_appPage;
    QSharedPointer<QWidget> m_teamsPage;

    QPair<QStringList, QStringList> m_availableProjects;
};

#endif // SETTINGSDIALOG_H
