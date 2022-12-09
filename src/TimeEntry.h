#ifndef TIMEENTRY_H
#define TIMEENTRY_H

#include <QDateTime>
#include <QObject>

#include <optional>

#include <nlohmann/json.hpp>

using nlohmann::json;

#include "Project.h"

class TimeEntry : public QObject
{
    Q_OBJECT

public:
    //! @param running If you know whether or not a time entry is running, pass true or false. Otherwise, pass std::nullopt.
    TimeEntry(const QString &id,
              const Project &project,
              const QString &userId,
              const QDateTime &start,
              const QDateTime &end,
              std::optional<bool> running,
              json extra = {},
              QObject *parent = nullptr);
    TimeEntry(const TimeEntry &that);
    TimeEntry(QObject *parent = nullptr);

    QString id() const { return m_id; }
    Project project() const { return m_project; }
    QString userId() const { return m_userId; }
    QDateTime start() const { return m_start; }
    QDateTime end() const { return m_end; }
    std::optional<bool> running() const { return m_running; }

    void setProject(const Project &project) { m_project = project; }
    void setStart(const QDateTime &start) { m_start = start; }
    void setEnd(const QDateTime &end) { m_end = end; }
    void setRunning(std::optional<bool> state) { m_running = state; }

    bool isValid() const { return m_isValid; }

    TimeEntry &operator=(const TimeEntry &other);
    bool operator==(const TimeEntry &other) const { return m_id == other.m_id; }

    //! Use this to store extra data about the entry (especially info about running time entries).
    json extraData;

private:
    QString m_id;
    Project m_project;
    QString m_userId;
    QDateTime m_start;
    QDateTime m_end;
    std::optional<bool> m_running;

    bool m_isValid{false};
};

#endif // TIMEENTRY_H
