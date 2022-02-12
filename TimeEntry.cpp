#include "TimeEntry.h"

#include "ClockifyManager.h"
#include "JsonHelper.h"

TimeEntry::TimeEntry(nlohmann::json entry, QObject *parent)
	: QObject{parent}
{
	try
	{
		m_id = entry["id"].get<QString>();
		auto projectId = entry["projectId"].get<QString>();
		m_project = {projectId, ClockifyManager::instance()->projectName(projectId), entry["description"].get<QString>()};
		m_userId = entry["userId"].get<QString>();
		m_start = entry["timeInterval"]["start"].get<QDateTime>();
		m_end = entry["timeInterval"]["end"].get<QDateTime>();

		m_isValid = true;
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error while creating time entry: " << e.what() << std::endl;
		std::cerr << entry.dump(4);
	}
}

TimeEntry::TimeEntry()
	: QObject{nullptr},
	  m_isValid{false}
{
}

TimeEntry::TimeEntry(const TimeEntry &that)
	: QObject{that.parent()}
{
	*this = that;
}

TimeEntry &TimeEntry::operator=(const TimeEntry &other)
{
	m_id = other.m_id;
	m_project = other.m_project;
	m_userId = other.m_userId;
	m_start = other.m_start;
	m_end = other.m_end;
	m_isValid = other.m_isValid;

	return *this;
}
