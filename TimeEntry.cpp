#include "TimeEntry.h"

#include "JsonHelper.h"

TimeEntry::TimeEntry(nlohmann::json entry, QObject *parent)
	: QObject{parent}
{
	try
	{
		m_id = entry["id"].get<QString>();
		m_projectId = entry["projectId"].get<QString>();
		m_description = entry["description"].get<QString>();
		m_userId = entry["userId"].get<QString>();
		m_start = entry["timeInterval"]["start"].get<QDateTime>();
		m_end = entry["timeInterval"]["end"].get<QDateTime>();
	}
	catch (...)
	{

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
	m_projectId = other.m_projectId;
	m_description = other.m_description;
	m_userId = other.m_userId;
	m_start = other.m_start;
	m_end = other.m_end;

	return *this;
}
