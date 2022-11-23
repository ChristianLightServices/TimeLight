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
    breakdownLayout->addWidget(breakdown);
    breakdownLayout->addStretch();
    layout->addLayout(breakdownLayout);

    auto chronologicalTable = new QTableWidget{static_cast<int>(todaysTime.count()), 3, this};
    for (int i = 0; i < todaysTime.size(); ++i)
    {
        chronologicalTable->setItem(i, 0, new TimeTableItem{todaysTime[i].start()});
        chronologicalTable->setItem(i, 1, new TimeTableItem{todaysTime[i].end()});
        chronologicalTable->setItem(i, 2, new QTableWidgetItem{todaysTime[i].project().name()});
    }
    chronologicalTable->setHorizontalHeaderLabels(QStringList{} << tr("Start") << tr("End") << tr("Project"));
    chronologicalTable->setEditTriggers(QTableWidget::NoEditTriggers);
    chronologicalTable->verticalHeader()->setVisible(false);
    chronologicalTable->horizontalHeader()->setStretchLastSection(true);
    //    timeTable->sortItems(0, Qt::AscendingOrder); // TODO: This no worky either!
    chronologicalTable->setSelectionMode(QTableWidget::NoSelection);
    layout->addWidget(chronologicalTable);

    QHash<QString, int> msecsByProject;
    for (int i = 0; i < todaysTime.size(); ++i)
        msecsByProject[todaysTime[i].project().id()] += todaysTime[i].start().msecsTo(todaysTime[i].end());
    auto byProjectTable = new QTableWidget{static_cast<int>(msecsByProject.count()), 2, this};
    int _i = 0;
    for (const auto &key : msecsByProject.keys())
    {
        byProjectTable->setItem(
            _i, 0, new QTableWidgetItem{QTime::fromMSecsSinceStartOfDay(msecsByProject[key]).toString("h:mm:ss")});
        byProjectTable->setItem(_i, 1, new QTableWidgetItem{m_manager->projectName(key)});
        ++_i;
    }
    byProjectTable->setHorizontalHeaderLabels(QStringList{} << tr("Start") << tr("End") << tr("Project"));
    byProjectTable->setEditTriggers(QTableWidget::NoEditTriggers);
    byProjectTable->verticalHeader()->setVisible(false);
    byProjectTable->horizontalHeader()->setStretchLastSection(true);
    //    timeTable->sortItems(0, Qt::AscendingOrder); // TODO: This no worky either!
    byProjectTable->setSelectionMode(QTableWidget::NoSelection);
    layout->addWidget(byProjectTable);

    auto bb = new QDialogButtonBox{QDialogButtonBox::Ok, this};
    layout->addWidget(bb, 0, Qt::AlignRight);

    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::close);

    auto setProperTimeTable = [breakdown, chronologicalTable, byProjectTable] {
        if (breakdown->currentData(Qt::UserRole).value<BreakDownOption>() == BreakDownOption::Chronologically)
        {
            chronologicalTable->setVisible(true);
            byProjectTable->setVisible(false);
        }
        else
        {
            chronologicalTable->setVisible(false);
            byProjectTable->setVisible(true);
        }
    };
    connect(breakdown, &QComboBox::currentIndexChanged, this, setProperTimeTable);
    setProperTimeTable();

    resize(600, 400);
}
