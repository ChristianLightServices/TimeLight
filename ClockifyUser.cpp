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

json ClockifyUser::getRunningTimeEntry() const
{
	return m_manager->getRunningTimeEntry(m_userId);
}

void ClockifyUser::stopCurrentTimeEntry(bool async)
{
	m_manager->stopRunningTimeEntry(m_userId, async);
}

void ClockifyUser::startTimeEntry(const QString &projectId, bool async) const
{
	m_manager->startTimeEntry(m_userId, projectId, async);
}

void ClockifyUser::startTimeEntry(const QString &projectId, const QString &description, bool async) const
{
	m_manager->startTimeEntry(m_userId, projectId, description, async);
}
