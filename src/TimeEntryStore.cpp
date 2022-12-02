#include "TimeEntryStore.h"

TimeEntryStore::TimeEntryStore(QSharedPointer<User> user, QObject *parent)
    : QObject{parent},
      m_user{user},
      m_nextPage{m_user->manager()->paginationStartsAt()}
{
    fetchMore();
}

void TimeEntryStore::fetchMore()
{
    auto moreEntries = m_user->getTimeEntries(m_nextPage++);
    if (moreEntries.isEmpty())
        m_isAtEnd = true;
    else
        m_store.append(moreEntries);
}

bool TimeEntryStore::insert(const TimeEntry &t)
{
    if (m_store.contains(t))
        return false;

    if (auto it = std::find_if(m_store.cbegin(), m_store.cend(), [&t](const auto &x) { return x.start() > t.start(); });
        it != m_store.cend())
        m_store.insert(it, t);
    else if (t.start() > m_store.first().start())
        m_store.prepend(t);
    else
        m_store.append(t);
    return true;
}

TimeEntryStore::iterator &TimeEntryStore::iterator::operator++()
{
    m_index++;

    if (m_index == m_parent->m_store.size() && m_autoload && !m_parent->m_isAtEnd &&
        m_parent->m_user->manager()->supportedPagination().testFlag(AbstractTimeServiceManager::Pagination::TimeEntries))
        m_parent->fetchMore();
    return *this;
}

TimeEntryStore::iterator &TimeEntryStore::iterator::operator--()
{
    m_index--;
    return *this;
}

bool TimeEntryStore::iterator::operator!=(const iterator &other)
{
    return m_parent != other.m_parent || m_index != other.m_index;
}

TimeEntryStore::iterator::iterator(qsizetype index, TimeEntryStore *store)
    : m_index{index},
      m_parent{store}
{}

TimeEntryStore::const_iterator &TimeEntryStore::const_iterator::operator++()
{
    m_index++;

    if (m_index == m_parent->m_store.size() && m_autoload && !m_parent->m_isAtEnd &&
        m_parent->m_user->manager()->supportedPagination().testFlag(AbstractTimeServiceManager::Pagination::TimeEntries))
        m_parent->fetchMore();
    return *this;
}

TimeEntryStore::const_iterator &TimeEntryStore::const_iterator::operator--()
{
    m_index--;
    return *this;
}

bool TimeEntryStore::const_iterator::operator!=(const const_iterator &other)
{
    return m_parent != other.m_parent || m_index != other.m_index;
}

TimeEntryStore::const_iterator::const_iterator(qsizetype index, TimeEntryStore *store)
    : m_index{index},
      m_parent{store}
{}
