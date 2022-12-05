#ifndef TIMEENTRYSTORE_H
#define TIMEENTRYSTORE_H

#include <QObject>

#include "TimeEntry.h"
#include "User.h"

class TimeEntryStore : public QObject
{
    Q_OBJECT

public:
    explicit TimeEntryStore(QSharedPointer<User> user, QObject *parent = nullptr);

    void fetchMore();
    void clearStore();

    const TimeEntry &at(qsizetype i) { return m_store.at(i); }
    qsizetype size() const { return m_store.size(); }
    bool insert(const TimeEntry &t);

    class iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = TimeEntry;
        using difference_type = void;
        using pointer = TimeEntry *;
        using reference = TimeEntry &;

        iterator &operator++();
        iterator &operator--();
        iterator &operator+(int i);
        iterator &operator-(int i);
        bool operator!=(const iterator &other);
        bool operator==(const iterator &other);
        TimeEntry &operator*() { return m_parent->m_store[m_index]; }
        TimeEntry *operator->() { return &m_parent->m_store[m_index]; }

        bool isAutoloadingEntries() const { return m_autoload; }
        void setAutoloadEntries(bool b) { m_autoload = b; }

    private:
        iterator(qsizetype index, TimeEntryStore *store);

        void maybeFetchMore();

        qsizetype m_index;
        TimeEntryStore *m_parent;
        bool m_autoload{true};

        friend class TimeEntryStore;
    };

    iterator begin() noexcept { return iterator{0, this}; }
    iterator end() noexcept { return iterator{m_store.size(), this}; }

    class const_iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = const TimeEntry;
        using difference_type = void;
        using pointer = const TimeEntry *;
        using reference = const TimeEntry &;

        const_iterator &operator++();
        const_iterator &operator--();
        const_iterator &operator+(int i);
        const_iterator &operator-(int i);
        bool operator!=(const const_iterator &other);
        bool operator==(const const_iterator &other);
        const TimeEntry &operator*() { return m_parent->m_store[m_index]; }
        const TimeEntry *operator->() { return &m_parent->m_store[m_index]; }

        bool isAutoloadingEntries() const { return m_autoload; }
        void setAutoloadEntries(bool b) { m_autoload = b; }

    private:
        const_iterator(qsizetype index, TimeEntryStore *store);

        void maybeFetchMore();

        qsizetype m_index;
        TimeEntryStore *m_parent;
        bool m_autoload{true};

        friend class TimeEntryStore;
    };

    const_iterator cbegin() noexcept { return const_iterator{0, this}; }
    const_iterator cend() noexcept { return const_iterator{m_store.size(), this}; }

private:
    QSharedPointer<User> m_user;

    QList<TimeEntry> m_store;
    int m_nextPage{};
    bool m_isAtEnd{false};

    friend class iterator;
};

#endif // TIMEENTRYSTORE_H
