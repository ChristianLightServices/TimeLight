#include "ClockifyWorkspace.h"

ClockifyWorkspace::ClockifyWorkspace(const QString &id, const QString &name, QObject *parent)
	: QObject{parent},
	  m_id{id},
	  m_name{name}
{

}

ClockifyWorkspace::ClockifyWorkspace(const ClockifyWorkspace &other)
	: QObject{other.parent()},
	  m_id{other.m_id},
	  m_name{other.m_name}
{

}
