#include "DailyOverviewDialog.h"

#include <QCalendarWidget>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QThread>

#include "Logger.h"
#include "TimeEntry.h"

DailyOverviewDialog::DailyOverviewDialog(QSharedPointer<AbstractTimeServiceManager> manager,
                                         QSharedPointer<User> user,
                                         QSharedPointer<TimeEntryStore> entryStore,
                                         QWidget *parent)
    : QDialog{parent},
      m_manager{manager},
      m_entries{entryStore},
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
    m_chronologicalTable->setSelectionMode(QTableWidget::NoSelection);

    m_byProjectTable->setHorizontalHeaderLabels(QStringList{} << tr("Duration") << tr("Project"));
    m_byProjectTable->setEditTriggers(QTableWidget::NoEditTriggers);
    m_byProjectTable->verticalHeader()->setVisible(false);
    m_byProjectTable->horizontalHeader()->setStretchLastSection(true);
    m_byProjectTable->setSelectionMode(QTableWidget::NoSelection);

    auto datePicker = new QDateEdit{this};
    datePicker->setDate(m_day);
    datePicker->setMaximumDate(m_manager->currentDateTime().date());
    datePicker->setCalendarPopup(true);
    datePicker->setDisplayFormat(QStringLiteral("MMMM d, yyyy"));

    m_loadingEntries = new QProgressBar{this};
    m_loadingEntries->setMinimum(0);
    m_loadingEntries->setMaximum(0);
    m_loadingEntries->setVisible(false);

    auto today = new QPushButton{tr("Today"), this};

    QIcon backIcon{QStringLiteral(":/icons/arrow-left.svg")};
    QIcon forwardIcon{QStringLiteral(":/icons/arrow-right.svg")};
    backIcon.setIsMask(true);
    forwardIcon.setIsMask(true);
    auto back = new QPushButton{QIcon::fromTheme(QStringLiteral("go-previous"), backIcon), {}, this};
    auto forward = new QPushButton{QIcon::fromTheme(QStringLiteral("go-next"), forwardIcon), {}, this};
    forward->setDisabled(true);

    auto dayLayout = new QHBoxLayout;
    dayLayout->addWidget(m_totalTime);
    dayLayout->addStretch();
    dayLayout->addWidget(m_loadingEntries, 1);
    dayLayout->addWidget(today);
    dayLayout->addWidget(back);
    dayLayout->addWidget(datePicker);
    dayLayout->addWidget(forward);

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
    auto reset = bb->addButton(tr("Refresh"), QDialogButtonBox::ButtonRole::ActionRole);
    m_layout->addWidget(bb, 0, Qt::AlignRight);

    auto updateTotalTime = [this] {
        auto todaysTime = m_entries->constSliceByDate(QDateTime{m_day, QTime{}}, m_day.endOfDay());

        m_totalTime->setText(
            tr("Total time: ")
                .append(QTime::fromMSecsSinceStartOfDay(std::accumulate(todaysTime.begin(),
                                                                        todaysTime.end(),
                                                                        0,
                                                                        [this](int a, const TimeEntry &b) -> int {
                                                                            return a + b.start().msecsTo(
                                                                                           b.end().isValid() ?
                                                                                               b.end() :
                                                                                               m_manager->currentDateTime());
                                                                        }))
                            .toString(QStringLiteral("h:mm:ss"))));
    };

    auto totalTimeUpdater = new QTimer{this};
    totalTimeUpdater->setInterval(1000);
    totalTimeUpdater->callOnTimeout(updateTotalTime);
    totalTimeUpdater->start();

    auto updateData = [this, datePicker, forward, back, today, reset, updateTotalTime, totalTimeUpdater] {
        auto todaysTime = m_entries->constSliceByDate(QDateTime{m_day, QTime{}}, m_day.endOfDay());

        updateTotalTime();

        m_chronologicalTable->clearContents();
        m_chronologicalTable->setRowCount(TimeLight::rangeSize(todaysTime));
        for (auto [i, it] = std::tuple(m_chronologicalTable->rowCount() - 1, todaysTime.begin()); it != todaysTime.end();
             --i, ++it)
        {
            m_chronologicalTable->setItem(i,
                                          0,
                                          new QTableWidgetItem{it->start().isValid() ?
                                                                   it->start().toString(QStringLiteral("h:mm:ss A")) :
                                                                   QObject::tr("Running")});
            m_chronologicalTable->setItem(i,
                                          1,
                                          new QTableWidgetItem{it->end().isValid() ?
                                                                   it->end().toString(QStringLiteral("h:mm:ss A")) :
                                                                   QObject::tr("Running")});
            m_chronologicalTable->setItem(i, 2, new QTableWidgetItem{it->project().name()});
        }

        QHash<QString, int> msecsByProject;
        for (const auto &t : todaysTime)
            msecsByProject[t.project().id()] += t.start().msecsTo(t.end());
        m_byProjectTable->clearContents();
        m_byProjectTable->setRowCount(0);
        m_byProjectTable->setRowCount(static_cast<int>(msecsByProject.count()));
        int _i = 0;
        for (const auto &key : msecsByProject.keys())
        {
            m_byProjectTable->setItem(
                _i,
                0,
                new QTableWidgetItem{
                    QTime::fromMSecsSinceStartOfDay(msecsByProject[key]).toString(QStringLiteral("h:mm:ss"))});
            m_byProjectTable->setItem(_i, 1, new QTableWidgetItem{m_manager->projectName(key)});
            ++_i;
        }

        m_loadingEntries->setVisible(false);
        if (m_day == datePicker->maximumDate())
            forward->setDisabled(true);
        else
            forward->setEnabled(true);
        if (m_day == datePicker->minimumDate())
            back->setDisabled(true);
        else
            back->setEnabled(true);
        datePicker->setEnabled(true);
        today->setEnabled(true);
        reset->setEnabled(true);
        totalTimeUpdater->start();
    };
    updateData();

    auto lockUIAndUpdate = [this, totalTimeUpdater, forward, back, datePicker, today, reset, updateData] {
        totalTimeUpdater->stop();
        forward->setDisabled(true);
        back->setDisabled(true);
        datePicker->setDisabled(true);
        today->setDisabled(true);
        reset->setDisabled(true);
        m_loadingEntries->setVisible(true);

        QTimer::singleShot(0, [updateData] { updateData(); });
    };

    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::close);
    connect(breakdown, &QComboBox::currentIndexChanged, this, setProperTimeTable);
    connect(datePicker,
            &QDateEdit::dateChanged,
            this,
            [this, lockUIAndUpdate](const QDate &d) {
                m_day = d;
                lockUIAndUpdate();
            });
    connect(back, &QPushButton::clicked, datePicker, [datePicker] { datePicker->setDate(datePicker->date().addDays(-1)); });
    connect(
        forward, &QPushButton::clicked, datePicker, [datePicker] { datePicker->setDate(datePicker->date().addDays(1)); });
    connect(today, &QPushButton::clicked, datePicker, [this, datePicker] {
        datePicker->setDate(m_manager->currentDateTime().date());
    });
    connect(reset, &QPushButton::clicked, this, [this, lockUIAndUpdate] {
        m_entries->clearStore(std::find_if(m_entries->static_cbegin(), m_entries->static_cend(), [this](const TimeEntry &t) {
            return t.start() <= m_day.endOfDay();
        }));
        lockUIAndUpdate();
    });

    resize(600, 400);
}
