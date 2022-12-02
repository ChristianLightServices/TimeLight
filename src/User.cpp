#include "User.h"

User::User(const QString &userId,
           const QString &name,
           const QString &workspaceId,
           const QString &avatarUrl,
           const QString &email,
           QSharedPointer<AbstractTimeServiceManager> parent)
    : QObject{parent.data()},
      m_userId{userId},
      m_name{name},
      m_workspaceId{workspaceId},
      m_avatarUrl{avatarUrl},
      m_email{email},
      m_manager{parent}
{}

User::User(const User &that)
    : QObject{that.parent()}
{
    *this = that;
}

User::User(QObject *parent)
    : QObject{parent},
      m_isValid{false}
{}

std::optional<TimeEntry> User::getRunningTimeEntry()
{
    return m_manager->getRunningTimeEntry(m_userId);
}

QDateTime User::stopCurrentTimeEntry(bool async) const
{
    return m_manager->stopRunningTimeEntry(m_userId, async);
}

void User::startTimeEntry(const Project &project, bool async) const
{
    m_manager->startTimeEntry(m_userId, project, async);
}

void User::startTimeEntry(const Project &project, QDateTime start, bool async) const
{
    m_manager->startTimeEntry(m_userId, project, start, async);
}

void User::modifyTimeEntry(const TimeEntry &t, bool async)
{
    m_manager->modifyTimeEntry(m_userId, t, async);
}

void User::deleteTimeEntry(const TimeEntry &t, bool async)
{
    m_manager->deleteTimeEntry(m_userId, t, async);
}

QVector<TimeEntry> User::getTimeEntries(std::optional<int> pageNumber,
                                        std::optional<int> pageSize,
                                        std::optional<QDateTime> start,
                                        std::optional<QDateTime> end)
{
    return m_manager->getTimeEntries(m_userId, pageNumber, pageSize, start, end);
}

User &User::operator=(const User &other)
{
    m_userId = other.m_userId;
    m_name = other.m_name;
    m_workspaceId = other.m_workspaceId;
    m_avatarUrl = other.m_avatarUrl;
    m_email = other.m_email;
    m_manager = other.m_manager;
    m_isValid = other.m_isValid;

    return *this;
}
