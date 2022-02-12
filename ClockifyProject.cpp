#include "ClockifyProject.h"

#include "JsonHelper.h"

ClockifyProject::ClockifyProject(const QString &id, const QString &name, const QString &description, QObject *parent)
	: QObject{parent},
	  m_id{id},
	  m_name{name},
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

ClockifyProject &ClockifyProject::operator=(const ClockifyProject &other)
{
	m_id = other.m_id;
	m_name = other.m_name;
	m_description = other.m_description;

	return *this;
}
