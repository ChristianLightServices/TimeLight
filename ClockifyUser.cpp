#include "ClockifyUser.h"

#include <nlohmann/json.hpp>

#include "ClockifyManager.h"

using json = nlohmann::json;

ClockifyUser::ClockifyUser(QString userId, QObject *parent)
	: QObject{parent},
	  m_userId{userId},
	  m_name{ClockifyManager::instance()->userName(userId)}
{
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

QVector<TimeEntry> ClockifyUser::getTimeEntries()
{
	return ClockifyManager::instance()->getTimeEntries(m_userId);
}
