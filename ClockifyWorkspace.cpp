#include "ClockifyWorkspace.h"

ClockifyWorkspace::ClockifyWorkspace(const QString &id, const QString &name, QObject *parent)
	: QObject{parent},
	  m_id{id},
	  m_name{name}
{

}

ClockifyWorkspace::ClockifyWorkspace(const ClockifyWorkspace &that)
	: QObject{that.parent()}
{
	*this = that;
}

ClockifyWorkspace &ClockifyWorkspace::operator=(const ClockifyWorkspace &other)
{
	this->m_id = other.m_id;
	this->m_name = other.m_name;

	return *this;
}
