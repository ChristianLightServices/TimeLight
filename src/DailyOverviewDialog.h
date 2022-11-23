#ifndef DAILYOVERVIEWDIALOG_H
#define DAILYOVERVIEWDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include "AbstractTimeServiceManager.h"
#include "User.h"

class DailyOverviewDialog : public QDialog
{
    Q_OBJECT

public:
    DailyOverviewDialog(AbstractTimeServiceManager *manager, User *user, QWidget *parent = nullptr);

private:
    enum class BreakDownOption
    {
        Chronologically,
        ByProject,
    };

    AbstractTimeServiceManager *m_manager;
    User *m_user;

    QLabel *m_totalTime;
    QTableWidget *m_chronologicalTable;
    QTableWidget *m_byProjectTable;

    QVBoxLayout *m_layout;
    QDate m_day;
};

#endif // DAILYOVERVIEWDIALOG_H
