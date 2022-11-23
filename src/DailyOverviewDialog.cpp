#include "DailyOverviewDialog.h"

#include <QCalendarWidget>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>

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
      m_user{user},
      m_totalTime{new QLabel},
      m_chronologicalTable{new QTableWidget{0, 3, this}},
      m_byProjectTable{new QTableWidget{0, 2, this}},
      m_day{m_manager->currentDateTime().date()}
{
    m_layout = new QVBoxLayout{this};

    m_chronologicalTable->setHorizontalHeaderLabels(QStringList{} << tr("Start") << tr("End") << tr("Project"));
    m_chronologicalTable->setEditTriggers(QTableWidget::NoEditTriggers);
    m_chronologicalTable->verticalHeader()->setVisible(false);
    m_chronologicalTable->horizontalHeader()->setStretchLastSection(true);
    //    timeTable->sortItems(0, Qt::AscendingOrder); // TODO: This no worky either!
    m_chronologicalTable->setSelectionMode(QTableWidget::NoSelection);

    m_byProjectTable->setHorizontalHeaderLabels(QStringList{} << tr("Duration") << tr("Project"));
    m_byProjectTable->setEditTriggers(QTableWidget::NoEditTriggers);
    m_byProjectTable->verticalHeader()->setVisible(false);
    m_byProjectTable->horizontalHeader()->setStretchLastSection(true);
    m_byProjectTable->setSelectionMode(QTableWidget::NoSelection);

    auto updateData = [this] {
        auto now = m_manager->currentDateTime();
        auto todaysTime = m_user->getTimeEntries(std::nullopt, std::nullopt, QDateTime{m_day, QTime{}}, m_day.endOfDay());
        std::reverse(todaysTime.begin(), todaysTime.end());

        m_totalTime->setText(
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
                            .toString("h:mm:ss")));

        m_chronologicalTable->clearContents();
        m_chronologicalTable->setRowCount(static_cast<int>(todaysTime.count()));
        for (int i = 0; i < todaysTime.size(); ++i)
        {
            m_chronologicalTable->setItem(i, 0, new TimeTableItem{todaysTime[i].start()});
            m_chronologicalTable->setItem(i, 1, new TimeTableItem{todaysTime[i].end()});
            m_chronologicalTable->setItem(i, 2, new QTableWidgetItem{todaysTime[i].project().name()});
        }

        QHash<QString, int> msecsByProject;
        for (int i = 0; i < todaysTime.size(); ++i)
            msecsByProject[todaysTime[i].project().id()] += todaysTime[i].start().msecsTo(todaysTime[i].end());
        m_byProjectTable->clearContents();
        m_byProjectTable->setRowCount(static_cast<int>(msecsByProject.count()));
        int _i = 0;
        for (const auto &key : msecsByProject.keys())
        {
            m_byProjectTable->setItem(
                _i, 0, new QTableWidgetItem{QTime::fromMSecsSinceStartOfDay(msecsByProject[key]).toString("h:mm:ss")});
            m_byProjectTable->setItem(_i, 1, new QTableWidgetItem{m_manager->projectName(key)});
            ++_i;
        }
    };
    updateData();

    auto datePicker = new QDateEdit{this};
    datePicker->setDate(m_day);
    datePicker->setMaximumDate(m_manager->currentDateTime().date());
    datePicker->setCalendarPopup(true);
    datePicker->setDisplayFormat(QStringLiteral("MMMM d, yyyy"));

    auto dayLayout = new QHBoxLayout;
    dayLayout->addWidget(m_totalTime);
    dayLayout->addStretch();
    dayLayout->addWidget(datePicker);

    m_layout->addLayout(dayLayout);
    m_layout->addWidget(m_chronologicalTable);
    m_layout->addWidget(m_byProjectTable);

    auto breakdownLayout = new QHBoxLayout;
    breakdownLayout->addWidget(new QLabel{tr("Break down time: ")});

    auto breakdown = new QComboBox{this};
    breakdown->addItem(tr("Chronologically"), static_cast<int>(BreakDownOption::Chronologically));
    breakdown->addItem(tr("By project"), static_cast<int>(BreakDownOption::ByProject));
    breakdownLayout->addWidget(breakdown);
    breakdownLayout->addStretch();
    m_layout->addLayout(breakdownLayout);

    auto setProperTimeTable = [this, breakdown] {
        if (breakdown->currentData(Qt::UserRole).value<BreakDownOption>() == BreakDownOption::Chronologically)
        {
            m_chronologicalTable->setVisible(true);
            m_byProjectTable->setVisible(false);
        }
        else
        {
            m_chronologicalTable->setVisible(false);
            m_byProjectTable->setVisible(true);
        }
    };
    setProperTimeTable();

    auto bb = new QDialogButtonBox{QDialogButtonBox::Ok, this};
    connect(bb->addButton(tr("Refresh"), QDialogButtonBox::ButtonRole::ActionRole), &QPushButton::clicked, this, updateData);
    m_layout->addWidget(bb, 0, Qt::AlignRight);

    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::close);
    connect(breakdown, &QComboBox::currentIndexChanged, this, setProperTimeTable);
    connect(datePicker, &QDateEdit::dateChanged, this, [this, updateData](const QDate &d) {
        m_day = d;
        updateData();
    });

    resize(600, 400);
}
