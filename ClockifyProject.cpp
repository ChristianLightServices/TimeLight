#include "ClockifyProject.h"

#include "ClockifyManager.h"
#include "JsonHelper.h"

ClockifyProject::ClockifyProject(QString id, QString description, QObject *parent)
	: QObject{parent},
	  m_id{id},
	  m_description{description}
{

}

ClockifyProject::ClockifyProject(const ClockifyProject &that)
	: QObject{that.parent()}
{
	*this = that;
}

ClockifyProject::ClockifyProject()
	: QObject{nullptr}
{
}

QString ClockifyProject::name()
{
	if (!m_nameLoaded)
	{
		m_name = ClockifyManager::instance()->projectName(m_id);
		m_nameLoaded = true;
	}

	return m_name;
}

ClockifyProject &ClockifyProject::operator=(const ClockifyProject &other)
{
	m_id = other.m_id;
	m_name = other.m_name;
	m_description = other.m_description;

	return *this;
}
