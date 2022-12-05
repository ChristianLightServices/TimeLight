#include "TimeEntryStore.h"

#include <QMutexLocker>

TimeEntryStore::TimeEntryStore(QSharedPointer<User> user, QObject *parent)
    : QObject{parent},
      m_user{user},
      m_nextPage{m_user->manager()->paginationStartsAt()}
{
    fetchMore();

    connect(&m_expiryTimer, &QTimer::timeout, this, &TimeEntryStore::clearStore);
    m_expiryTimer.setInterval(3 * 60 * 60 * 1000);
    m_expiryTimer.start();
}

void TimeEntryStore::fetchMore()
{
    QMutexLocker _{&m_mut};
    auto moreEntries = m_user->getTimeEntries(m_nextPage++);
    if (moreEntries.isEmpty())
        m_isAtEnd = true;
    else
        m_store.append(moreEntries);
}

void TimeEntryStore::clearStore()
{
    QMutexLocker _{&m_mut};
    m_store.clear();
    m_nextPage = m_user->manager()->paginationStartsAt();
    m_isAtEnd = false;
}

void TimeEntryStore::insert(const TimeEntry &t)
{
    QMutexLocker _{&m_mut};
    if (auto it = std::find_if(m_store.begin(), m_store.end(), [&t](const auto &x) { return t.id() == x.id(); }); it != m_store.end())
        *it = t;
    else if (auto it = std::find_if(m_store.cbegin(), m_store.cend(), [&t](const auto &x) { return x.start() > t.start(); });
        it != m_store.cend())
        m_store.insert(it, t);
    else if (t.start() > m_store.first().start())
        m_store.prepend(t);
    else
        m_store.append(t);
}

TimeEntryStore::iterator &TimeEntryStore::iterator::operator++()
{
    m_index++;
    maybeFetchMore();
    return *this;
}

TimeEntryStore::iterator &TimeEntryStore::iterator::operator--()
{
    m_index--;
    return *this;
}

TimeEntryStore::iterator &TimeEntryStore::iterator::operator+(int i)
{
    m_index += i;
    maybeFetchMore();
    return *this;
}

TimeEntryStore::iterator &TimeEntryStore::iterator::operator-(int i)
{
    m_index -= i;
    return *this;
}

bool TimeEntryStore::iterator::operator!=(const iterator &other)
{
    return m_parent != other.m_parent || m_index != other.m_index;
}

bool TimeEntryStore::iterator::operator==(const iterator &other)
{
    return m_parent == other.m_parent && m_index == other.m_index;
}

TimeEntryStore::iterator::iterator(qsizetype index, TimeEntryStore *store)
    : m_index{index},
      m_parent{store}
{
    maybeFetchMore();
}

void TimeEntryStore::iterator::maybeFetchMore()
{
    if (m_index == m_parent->m_store.size() && m_autoload && !m_parent->m_isAtEnd &&
        m_parent->m_user->manager()->supportedPagination().testFlag(AbstractTimeServiceManager::Pagination::TimeEntries))
        m_parent->fetchMore();
}

TimeEntryStore::const_iterator &TimeEntryStore::const_iterator::operator++()
{
    m_index++;
    maybeFetchMore();
    return *this;
}

TimeEntryStore::const_iterator &TimeEntryStore::const_iterator::operator--()
{
    m_index--;
    return *this;
}

TimeEntryStore::const_iterator &TimeEntryStore::const_iterator::operator+(int i)
{
    m_index += i;
    maybeFetchMore();
    return *this;
}

TimeEntryStore::const_iterator &TimeEntryStore::const_iterator::operator-(int i)
{
    m_index -= i;
    return *this;
}

bool TimeEntryStore::const_iterator::operator!=(const const_iterator &other)
{
    return m_parent != other.m_parent || m_index != other.m_index;
}

bool TimeEntryStore::const_iterator::operator==(const const_iterator &other)
{
    return m_parent == other.m_parent && m_index == other.m_index;
}

TimeEntryStore::const_iterator::const_iterator(qsizetype index, TimeEntryStore *store)
    : m_index{index},
      m_parent{store}
{
    maybeFetchMore();
}

void TimeEntryStore::const_iterator::maybeFetchMore()
{
    if (m_index == m_parent->m_store.size() && m_autoload && !m_parent->m_isAtEnd &&
        m_parent->m_user->manager()->supportedPagination().testFlag(AbstractTimeServiceManager::Pagination::TimeEntries))
        m_parent->fetchMore();
}
