#include "DailyOverviewDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

#include "TimeEntry.h"

class TimeTableItem : public QTableWidgetItem
{
public:
    enum class ItemType
    {
        Time,
        Project,
    };

    TimeTableItem(const QDateTime &time)
        : QTableWidgetItem{time.isValid() ? time.toString(QStringLiteral("h:mm:ss A")) : QObject::tr("Running")}
    {
        setData(Qt::UserRole, time);
    }

    // TODO: This no worky!
    bool operator<(const QTableWidgetItem &other)
    {
        return this->data(Qt::UserRole).toDateTime() < other.data(Qt::UserRole).toDateTime();
    }
};

DailyOverviewDialog::DailyOverviewDialog(AbstractTimeServiceManager *manager, User *user, QWidget *parent)
    : QDialog{parent},
      m_manager{manager},
      m_user{user}
{
    auto now = m_manager->currentDateTime();
    auto todaysTime = m_user->getTimeEntries(std::nullopt, std::nullopt, QDateTime{now.date(), QTime{}}, now);
    std::reverse(todaysTime.begin(), todaysTime.end());

    auto layout = new QVBoxLayout{this};
    layout->addWidget(new QLabel{
        tr("Total time today: ")
            .append(QTime::fromMSecsSinceStartOfDay(std::accumulate(todaysTime.begin(),
                                                                    todaysTime.end(),
                                                                    0,
                                                                    [this](int a, const TimeEntry &b) -> int {
                                                                        return a + b.start().msecsTo(
                                                                                       b.end().isValid() ?
                                                                                           b.end() :
                                                                                           m_manager->currentDateTime());
                                                                    }))
                        .toString("h:mm:ss")),
        this});

    auto breakdownLayout = new QHBoxLayout;
    breakdownLayout->addWidget(new QLabel{tr("Break down time: ")});

    auto breakdown = new QComboBox{this};
    breakdown->addItem(tr("Chronologically"), static_cast<int>(BreakDownOption::Chronologically));
    breakdown->addItem(tr("By project"), static_cast<int>(BreakDownOption::ByProject));
    breakdown->setEnabled(false);
    breakdownLayout->addWidget(breakdown);
    breakdownLayout->addStretch();
    layout->addLayout(breakdownLayout);

    auto timeTable = new QTableWidget{static_cast<int>(todaysTime.count()), 3, this};
    for (int i = 0; i < todaysTime.size(); ++i)
    {
        timeTable->setItem(i, 0, new TimeTableItem{todaysTime[i].start()});
        timeTable->setItem(i, 1, new TimeTableItem{todaysTime[i].end()});
        timeTable->setItem(i, 2, new QTableWidgetItem{todaysTime[i].project().name()});
    }
    timeTable->setHorizontalHeaderLabels(QStringList{} << tr("Start") << tr("End") << tr("Project"));
    timeTable->setEditTriggers(QTableWidget::NoEditTriggers);
    timeTable->verticalHeader()->setVisible(false);
    timeTable->horizontalHeader()->setStretchLastSection(true);
    //    timeTable->sortItems(0, Qt::AscendingOrder); // TODO: This no worky either!
    timeTable->setSelectionMode(QTableWidget::NoSelection);
    layout->addWidget(timeTable);

    auto bb = new QDialogButtonBox{QDialogButtonBox::Ok, this};
    layout->addWidget(bb, 0, Qt::AlignRight);

    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::close);

    resize(700, 400);
}
