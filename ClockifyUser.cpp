#include "ClockifyUser.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

ClockifyUser::ClockifyUser(QString userId, ClockifyManager *manager)
	: QObject{manager},
	  m_manager{manager},
	  m_userId{userId},
	  m_name{m_manager->userName(userId)}
{
}

bool ClockifyUser::hasRunningTimeEntry()
{
	return m_manager->userHasRunningTimeEntry(m_userId);
}

json ClockifyUser::getRunningTimeEntry()
{
	return m_manager->getRunningTimeEntry(m_userId)[0];
}

QDateTime ClockifyUser::stopCurrentTimeEntry(bool async) const
{
	return m_manager->stopRunningTimeEntry(m_userId, async);
}

void ClockifyUser::startTimeEntry(const QString &projectId, bool async) const
{
	m_manager->startTimeEntry(m_userId, projectId, async);
}

void ClockifyUser::startTimeEntry(const QString &projectId, const QString &description, bool async) const
{
	m_manager->startTimeEntry(m_userId, projectId, description, async);
}

void ClockifyUser::startTimeEntry(const QString &projectId, QDateTime start, bool async) const
{
	m_manager->startTimeEntry(m_userId, projectId, start, async);
}

void ClockifyUser::startTimeEntry(const QString &projectId, const QString &description, QDateTime start, bool async) const
{
	m_manager->startTimeEntry(m_userId, projectId, description, start, async);
}

json ClockifyUser::getTimeEntries()
{
	return m_manager->getTimeEntries(m_userId);
}
