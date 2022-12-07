#ifndef DAILYOVERVIEWDIALOG_H
#define DAILYOVERVIEWDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include "AbstractTimeServiceManager.h"
#include "TimeEntryStore.h"
#include "User.h"

class DailyOverviewDialog : public QDialog
{
    Q_OBJECT

public:
    DailyOverviewDialog(QSharedPointer<AbstractTimeServiceManager> manager,
                        QSharedPointer<User> user,
                        QSharedPointer<TimeEntryStore> entryStore,
                        QWidget *parent = nullptr);

private:
    enum class BreakDownOption
    {
        Chronologically,
        ByProject,
    };

    QSharedPointer<AbstractTimeServiceManager> m_manager;
    QSharedPointer<TimeEntryStore> m_entries;

    QLabel *m_totalTime;
    QTableWidget *m_chronologicalTable;
    QTableWidget *m_byProjectTable;

    QVBoxLayout *m_layout;
    QDate m_day;
};

#endif // DAILYOVERVIEWDIALOG_H
