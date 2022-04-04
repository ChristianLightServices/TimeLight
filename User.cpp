#include "User.h"

#include "JsonHelper.h"
#include "ClockifyManager.h"

User::User(const QString &userId, const QString &name, const QString &workspaceId, AbstractTimeServiceManager *parent)
	: QObject{parent},
      m_userId{userId},
      m_name{name},
      m_workspaceId{workspaceId},
      m_manager{parent}
{
}

User::User(const User &that)
	: QObject{that.parent()}
{
	*this = that;
}

User::User(QObject *parent)
    : QObject{parent},
      m_isValid{false}
{
}

bool User::hasRunningTimeEntry()
{
	return m_manager->userHasRunningTimeEntry(m_userId);
}

TimeEntry User::getRunningTimeEntry()
{
	return m_manager->getRunningTimeEntry(m_userId);
}

QDateTime User::stopCurrentTimeEntry(bool async) const
{
	return m_manager->stopRunningTimeEntry(m_userId, async);
}

void User::startTimeEntry(const QString &projectId, bool async) const
{
	m_manager->startTimeEntry(m_userId, projectId, async);
}

void User::startTimeEntry(const QString &projectId, const QString &description, bool async) const
{
	m_manager->startTimeEntry(m_userId, projectId, description, async);
}

void User::startTimeEntry(const QString &projectId, QDateTime start, bool async) const
{
	m_manager->startTimeEntry(m_userId, projectId, start, async);
}

void User::startTimeEntry(const QString &projectId, const QString &description, QDateTime start, bool async) const
{
	m_manager->startTimeEntry(m_userId, projectId, description, start, async);
}

QVector<TimeEntry> User::getTimeEntries(std::optional<int> pageNumber, std::optional<int> pageSize)
{
	return m_manager->getTimeEntries(m_userId, pageNumber, pageSize);
}

User &User::operator=(const User &other)
{
	m_userId = other.m_userId;
	m_name = other.m_name;
	m_workspaceId = other.m_workspaceId;
	m_manager = other.m_manager;
	m_isValid = other.m_isValid;

	return *this;
}