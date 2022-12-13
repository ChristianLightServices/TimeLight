#ifndef TIMEENTRYSTORE_H
#define TIMEENTRYSTORE_H

#include <QAbstractListModel>
#include <QMutex>

#include "TimeEntry.h"
#include "User.h"
#include "Utils.h"

class TimeEntryStore : public QAbstractListModel, public QEnableSharedFromThis<TimeEntryStore>
{
    Q_OBJECT

public:
    enum Roles
    {
        Start = Qt::DisplayRole,
        Id,
        UserId,
        End,
        Project,
        IsRunning,
    };

    template<class Type>
    class iterator_base
    {
    public:
        iterator_base() {}

        template<class Other>
        iterator_base(const iterator_base<Other> &other)
            : m_index{other.m_index},
              m_parent{other.m_parent},
              m_autoload{other.m_autoload}
        {}

        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = Type;
        using difference_type = void;
        using pointer = Type *;
        using reference = Type &;

        iterator_base &operator++()
        {
            m_index++;
            maybeFetchMore();
            return *this;
        }
        iterator_base &operator--()
        {
            m_index--;
            return *this;
        }
        iterator_base operator++(int) const
        {
            auto ret = *this;
            ++*this;
            return ret;
        }
        iterator_base operator--(int) const
        {
            auto ret = *this;
            --*this;
            return ret;
        }
        iterator_base &operator+=(int i)
        {
            m_index += i;
            maybeFetchMore();
            return *this;
        }
        iterator_base &operator-=(int i)
        {
            m_index -= i;
            return *this;
        }
        iterator_base operator+(int i) const
        {
            auto ret = *this;
            ret += i;
            return ret;
        }
        iterator_base operator-(int i) const
        {
            auto ret = *this;
            ret -= i;
            return ret;
        }
        bool operator==(const iterator_base &other) const
        {
            if (m_parent != other.m_parent)
                return false;

            if (other.m_index < 0 || other.m_index >= m_parent->size())
            {
                if (m_index >= 0 && m_index < m_parent->size())
                    return false;
                else
                    return true;
            }
            else
                return m_index == other.m_index;
        }
        Type &operator*() const { return m_parent->m_store[m_index]; }
        Type *operator->() const { return &m_parent->m_store[m_index]; }

        bool isAutoloadingEntries() const { return m_autoload; }
        void setAutoloadEntries(bool b) { m_autoload = b; }
        qsizetype index() const noexcept { return m_index; }

    private:
        iterator_base(qsizetype index, QSharedPointer<TimeEntryStore> store, bool autoload = true)
            : m_index{index},
              m_parent{store},
              m_autoload{autoload}
        {
            maybeFetchMore();
        }

        void maybeFetchMore()
        {
            if (m_index == m_parent->m_store.size() && m_autoload && !m_parent->m_isAtEnd &&
                m_parent->m_user->manager()->supportedPagination().testFlag(
                    AbstractTimeServiceManager::Pagination::TimeEntries))
                m_parent->fetchMore();
        }

        qsizetype m_index{-1};
        QSharedPointer<TimeEntryStore> m_parent;
        bool m_autoload{true};

        friend class TimeEntryStore;
    };

    using iterator = iterator_base<TimeEntry>;
    using const_iterator = iterator_base<const TimeEntry>;

    explicit TimeEntryStore(QSharedPointer<User> user, QObject *parent = nullptr);

    int rowCount(const QModelIndex & = {}) const { return m_store.size(); }
    virtual QVariant data(const QModelIndex &index, int role) const;
    QHash<int, QByteArray> roleNames() const;

    void fetchMore();
    void clearStore();
    void clearStore(const_iterator deleteAt);

    TimeEntry &operator[](qsizetype i) { return m_store[i]; }
    const TimeEntry &at(qsizetype i) { return m_store.at(i); }
    qsizetype size() const { return m_store.size(); }
    void insert(const TimeEntry &t);

    iterator begin() noexcept { return iterator{0, sharedFromThis()}; }
    iterator end() noexcept { return iterator{-1, sharedFromThis()}; }
    const_iterator cbegin() noexcept { return const_iterator{0, sharedFromThis()}; }
    const_iterator cend() noexcept { return const_iterator{-1, sharedFromThis()}; }
    iterator static_begin() noexcept { return iterator{0, sharedFromThis(), false}; }
    iterator static_end() noexcept { return iterator{-1, sharedFromThis(), false}; }
    const_iterator static_cbegin() noexcept { return const_iterator{0, sharedFromThis(), false}; }
    const_iterator static_cend() noexcept { return const_iterator{-1, sharedFromThis(), false}; }

    auto rbegin() noexcept { return m_store.rbegin(); }
    auto rend() noexcept { return m_store.rend(); }
    auto crbegin() noexcept { return m_store.crbegin(); }
    auto crend() noexcept { return m_store.crend(); }

    RangeSlice<iterator> sliceByDate(const QDateTime &oldest, const QDateTime &newest);
    RangeSlice<const_iterator> constSliceByDate(const QDateTime &oldest, const QDateTime &newest);

private:
    QSharedPointer<User> m_user;

    QList<TimeEntry> m_store;
    QList<QPair<int, int>> m_pagesOccuredAtIndex;
    int m_nextPage{};
    bool m_isAtEnd{false};

    QTimer m_expiryTimer;
    QMutex m_mutex;

    friend class iterator_base<TimeEntry>;
    friend class iterator_base<const TimeEntry>;
};

#endif // TIMEENTRYSTORE_H
