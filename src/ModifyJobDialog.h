#ifndef MODIFYJOBDIALOG_H
#define MODIFYJOBDIALOG_H

#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialog>

#include "AbstractTimeServiceManager.h"
#include "TimeEntryStore.h"

class ModifyJobDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ModifyJobDialog(QSharedPointer<AbstractTimeServiceManager> manager,
                             const TimeEntry &entry,
                             QSharedPointer<TimeEntryStore> timeEntries = {},
                             QWidget *parent = nullptr);

    TimeEntry entry() const { return m_entry; }

private:
    QSharedPointer<AbstractTimeServiceManager> m_manager;
    TimeEntry m_entry;

    QList<Project> m_availableProjects;
};

#endif // MODIFYJOBDIALOG_H
