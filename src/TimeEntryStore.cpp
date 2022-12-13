#include "TimeEntryStore.h"

#include <QMutexLocker>
#include <QTableView>
#include <QThread>

#include "Logger.h"

TimeEntryStore::TimeEntryStore(QSharedPointer<User> user, QObject *parent)
    : QAbstractListModel{parent},
      m_user{user},
      m_nextPage{m_user->manager()->paginationStartsAt()}
{
    fetchMore();

    connect(&m_expiryTimer, &QTimer::timeout, this, [this] { clearStore(); });
    m_expiryTimer.setInterval(2 * 60 * 60 * 1000);
    m_expiryTimer.start();
}

QVariant TimeEntryStore::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_store.size())
        return {};

    switch (role)
    {
    case Roles::Id:
        return m_store[index.row()].id();
    case Roles::UserId:
        return m_store[index.row()].userId();
    case Roles::Start:
        return m_store[index.row()].start();
    case Roles::End:
        return m_store[index.row()].end();
    case Roles::Project:
        // return m_store[index.row()].project();
        return m_store[index.row()].project().name();
    case Roles::IsRunning:
        return m_store[index.row()].running();
    default:
        return {};
    }
}

QHash<int, QByteArray> TimeEntryStore::roleNames() const
{
    return {
        {Roles::Id, QByteArrayLiteral("id")},
        {Roles::UserId, QByteArrayLiteral("userId")},
        {Roles::Start, QByteArrayLiteral("start")},
        {Roles::End, QByteArrayLiteral("end")},
        {Roles::Project, QByteArrayLiteral("project")},
        {Roles::IsRunning, QByteArrayLiteral("isRunning")},
    };
}

void TimeEntryStore::fetchMore()
{
    if (m_isAtEnd)
        return;

    if (m_mutex.tryLock())
    {
        TimeLight::logs::app()->trace("TimeEntryStore::fetchMore() on thread {}: lock mutex", QThread::currentThreadId());
        m_pagesOccuredAtIndex.push_back({m_nextPage, m_store.size()});
        auto moreEntries = m_user->getTimeEntries(m_nextPage++);
        if (moreEntries.isEmpty())
        {
            m_isAtEnd = true;
            m_pagesOccuredAtIndex.pop_back();
        }
        else
        {
            beginInsertRows({}, m_store.size(), m_store.size() + moreEntries.size() - 1);
            m_store.append(moreEntries);
            endInsertRows();
        }
        m_mutex.unlock();
        TimeLight::logs::app()->trace("TimeEntryStore::fetchMore() on thread {}: unlock mutex", QThread::currentThreadId());
    }
    else
        QTimer::singleShot(0, this, &TimeEntryStore::fetchMore);
}

void TimeEntryStore::clearStore()
{
    clearStore({});
}

void TimeEntryStore::clearStore(const_iterator deleteAt)
{
    if (m_mutex.tryLock())
    {
        TimeLight::logs::app()->trace("TimeEntryStore::clearStore() on thread {}: lock mutex", QThread::currentThreadId());
        beginResetModel();
        m_nextPage = m_user->manager()->paginationStartsAt();
        if (deleteAt.index() >= 0 && deleteAt.index() < m_store.size())
        {
            m_pagesOccuredAtIndex.push_back({m_nextPage, m_store.size()});
            for (int i = 0; i < m_pagesOccuredAtIndex.size() - 1; ++i)
                if (m_pagesOccuredAtIndex[i + 1].second > deleteAt.index())
                {
                    m_nextPage = m_pagesOccuredAtIndex[i].first;
                    m_store.resize(m_pagesOccuredAtIndex[i].second);
                    break;
                }
        }
        else
        {
            m_store.clear();
            m_pagesOccuredAtIndex.clear();
        }
        endResetModel();
        m_isAtEnd = false;
        m_mutex.unlock();
        TimeLight::logs::app()->trace("TimeEntryStore::clearStore() on thread {}: unlock mutex", QThread::currentThreadId());
    }
    else
        QTimer::singleShot(0, this, [this, deleteAt] { clearStore(deleteAt); });
}

void TimeEntryStore::insert(const TimeEntry &t)
{
    if (m_mutex.tryLock())
    {
        TimeLight::logs::app()->trace("TimeEntryStore::insert() on thread {}: lock mutex", QThread::currentThreadId());
        if (auto it = std::find_if(static_begin(), static_end(), [&t](const auto &x) { return t.id() == x.id(); });
            it != static_end())
            *it = t;
        else if (auto it =
                     std::find_if(static_cbegin(), static_cend(), [&t](const auto &x) { return x.start() > t.start(); });
                 it != static_cend())
        {
            beginInsertRows({}, it.index(), it.index());
            m_store.insert(it.index(), t);
            endInsertRows();
        }
        else if (m_store.size() > 0 && t.start() > m_store.first().start())
        {
            beginInsertRows({}, 0, 0);
            m_store.prepend(t);
            endInsertRows();
        }
        else
        {
            beginInsertRows({}, m_store.size(), m_store.size());
            m_store.append(t);
            endInsertRows();
        }
        m_mutex.unlock();
        TimeLight::logs::app()->trace("TimeEntryStore::insert() on thread {}: unlock mutex", QThread::currentThreadId());
    }
    else
        QTimer::singleShot(0, std::bind(&TimeEntryStore::insert, this, std::move(t)));
}

RangeSlice<TimeEntryStore::iterator> TimeEntryStore::sliceByDate(const QDateTime &from, const QDateTime &to)
{
    auto start = std::find_if(begin(), end(), [&to, &from](const TimeEntry &t) { return t.start() <= std::max(to, from); });
    auto finish = std::find_if(start, end(), [&to, &from](const TimeEntry &t) { return t.start() < std::min(to, from); });
    return {start, finish};
}

RangeSlice<TimeEntryStore::const_iterator> TimeEntryStore::constSliceByDate(const QDateTime &from, const QDateTime &to)
{
    auto start =
        std::find_if(cbegin(), cend(), [&to, &from](const TimeEntry &t) { return t.start() <= std::max(to, from); });
    auto finish = std::find_if(start, cend(), [&to, &from](const TimeEntry &t) { return t.start() < std::min(to, from); });
    return {start, finish};
}
