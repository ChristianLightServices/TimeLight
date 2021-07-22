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
	bool success = false;

	while (!success)
	{
		try
		{
			m_hasRunningTimeEntry = m_manager->userHasRunningTimeEntry(m_userId);

			// if we get here, it means that the call succeeded with no errors
			success = true;
		}
		catch (...) {}
	}

	return m_hasRunningTimeEntry;
}

json ClockifyUser::getRunningTimeEntry()
{
	bool success = false;

	while (!success)
	{
		try
		{
			m_runningTimeEntry = m_manager->getRunningTimeEntry(m_userId);
			success = true;
		}
		catch (...) {}
	}

	return m_runningTimeEntry;
}

void ClockifyUser::stopCurrentTimeEntry(bool async)
{
	// we'll assume that all async calls succeed
	bool success = async;

	while (!success)
	{
		try
		{
			m_manager->stopRunningTimeEntry(m_userId, async);

			success = true;
		}
		catch (...) {}
	}
}

void ClockifyUser::startTimeEntry(const QString &projectId, bool async) const
{
	m_manager->startTimeEntry(m_userId, projectId, async);
}

void ClockifyUser::startTimeEntry(const QString &projectId, const QString &description, bool async) const
{
	m_manager->startTimeEntry(m_userId, projectId, description, async);
}
