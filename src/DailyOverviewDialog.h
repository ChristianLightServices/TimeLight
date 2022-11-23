#ifndef DAILYOVERVIEWDIALOG_H
#define DAILYOVERVIEWDIALOG_H

#include <QDialog>

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
};

#endif // DAILYOVERVIEWDIALOG_H
