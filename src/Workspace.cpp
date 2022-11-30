#include "Workspace.h"

Workspace::Workspace(const QString &id, const QString &name, QObject *parent)
    : QObject{parent},
      m_id{id},
      m_name{name}
{}

Workspace::Workspace(const Workspace &that)
    : QObject{that.parent()}
{
    *this = that;
}

Workspace &Workspace::operator=(const Workspace &other)
{
    this->m_id = other.m_id;
    this->m_name = other.m_name;
    this->m_valid = other.m_valid;

    return *this;
}

bool Workspace::operator==(const Workspace &other) const
{
    return m_id == other.m_id;
}
