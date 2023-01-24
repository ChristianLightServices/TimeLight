#include "Project.h"

#include "JsonHelper.h"

Project::Project(const QString &id, const QString &name, const QString &description, bool archived, QObject *parent)
    : QObject{parent},
      m_id{id},
      m_name{name},
      m_description{description},
      m_archived{archived}
{}

Project::Project(const QString &id, const QString &name, bool archived, QObject *parent)
    : Project{id, name, {}, archived, parent}
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
    m_archived = other.m_archived;

    return *this;
}

bool Project::operator==(const Project &other) const
{
    return m_id == other.m_id;
}
