#include "TimeEntry.h"

#include "ClockifyManager.h"
#include "JsonHelper.h"

TimeEntry::TimeEntry(const QString &id,
                     const Project &project,
                     const QString &description,
                     const QString &userId,
                     const QDateTime &start,
                     const QDateTime &end,
                     QObject *parent)
    : QObject{parent},
      m_id{id},
      m_project{project},
      m_description{description},
      m_userId{userId},
      m_start{start},
      m_end{end},
      m_isValid{true}
{}

TimeEntry::TimeEntry(QObject *parent) : QObject{parent} {}

TimeEntry::TimeEntry(const TimeEntry &that) : QObject{that.parent()}
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
