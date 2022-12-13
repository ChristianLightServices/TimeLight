#include "TimeEntry.h"

TimeEntry::TimeEntry(const QString &id,
                     const Project &project,
                     const QString &userId,
                     const QDateTime &start,
                     const QDateTime &end,
                     bool running,
                     json extra,
                     QObject *parent)
    : QObject{parent},
      m_id{id},
      m_project{project},
      m_userId{userId},
      m_start{start},
      m_end{end},
      m_running{running},
      extraData{extra},
      m_isValid{true}
{
    if (m_start.timeSpec() != Qt::LocalTime)
        m_start = m_start.toLocalTime();
    if (m_end.timeSpec() != Qt::LocalTime)
        m_end = m_end.toLocalTime();
}

TimeEntry::TimeEntry(QObject *parent)
    : QObject{parent}
{}

TimeEntry::TimeEntry(const TimeEntry &that)
    : QObject{that.parent()}
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
    m_running = other.m_running;
    extraData = other.extraData;
    m_isValid = other.m_isValid;

    return *this;
}
