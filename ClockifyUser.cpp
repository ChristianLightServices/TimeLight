#include "ClockifyUser.h"

#include <nlohmann/json.hpp>

#include "ClockifyManager.h"

using json = nlohmann::json;

ClockifyUser::ClockifyUser(const QString &userId, QObject *parent)
	: QObject{parent},
	  m_userId{userId},
	  m_name{ClockifyManager::instance()->userName(userId)}
{
}

ClockifyUser::ClockifyUser(const ClockifyUser &that)
	: QObject{that.parent()}
{
	*this = that;
}

bool ClockifyUser::hasRunningTimeEntry()
{
	return ClockifyManager::instance()->userHasRunningTimeEntry(m_userId);
}

TimeEntry ClockifyUser::getRunningTimeEntry()
{
	return ClockifyManager::instance()->getRunningTimeEntry(m_userId);
}

QDateTime ClockifyUser::stopCurrentTimeEntry(bool async) const
{
	return ClockifyManager::instance()->stopRunningTimeEntry(m_userId, async);
}

void ClockifyUser::startTimeEntry(const QString &projectId, bool async) const
{
	ClockifyManager::instance()->startTimeEntry(m_userId, projectId, async);
}

void ClockifyUser::startTimeEntry(const QString &projectId, const QString &description, bool async) const
{
	ClockifyManager::instance()->startTimeEntry(m_userId, projectId, description, async);
}

void ClockifyUser::startTimeEntry(const QString &projectId, QDateTime start, bool async) const
{
	ClockifyManager::instance()->startTimeEntry(m_userId, projectId, start, async);
}

void ClockifyUser::startTimeEntry(const QString &projectId, const QString &description, QDateTime start, bool async) const
{
	ClockifyManager::instance()->startTimeEntry(m_userId, projectId, description, start, async);
}

QVector<TimeEntry> ClockifyUser::getTimeEntries(std::optional<int> pageNumber, std::optional<int> pageSize)
{
	return ClockifyManager::instance()->getTimeEntries(m_userId, pageNumber, pageSize);
}

ClockifyUser &ClockifyUser::operator=(const ClockifyUser &other)
{
	m_userId = other.m_userId;
	m_name = other.m_name;

	return *this;
}
