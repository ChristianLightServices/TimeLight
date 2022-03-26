#include "Workspace.h"

Workspace::Workspace(const QString &id, const QString &name, QObject *parent)
	: QObject{parent},
	  m_id{id},
	  m_name{name}
{

}

Workspace::Workspace(const Workspace &that)
	: QObject{that.parent()}
{
	*this = that;
}

Workspace &Workspace::operator=(const Workspace &other)
{
	this->m_id = other.m_id;
	this->m_name = other.m_name;

	return *this;
}
