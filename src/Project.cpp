#include "Project.h"

#include "JsonHelper.h"

Project::Project(const QString &id, const QString &name, const QString &description, QObject *parent)
    : QObject{parent},
      m_id{id},
      m_name{name},
      m_description{description}
{}

Project::Project(const QString &id, const QString &name, QObject *parent)
    : Project{id, name, {}, parent}
{}

Project::Project(const Project &that)
    : QObject{that.parent()}
{
    *this = that;
}

Project::Project(QObject *parent)
    : QObject{parent}
{}

Project &Project::operator=(const Project &other)
{
    m_id = other.m_id;
    m_name = other.m_name;
    m_description = other.m_description;

    return *this;
}

bool Project::operator==(const Project &other) const
{
    return m_id == other.m_id && m_name == other.m_name && m_description == other.m_description;
}
