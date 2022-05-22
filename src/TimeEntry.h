#ifndef TIMEENTRY_H
#define TIMEENTRY_H

#include <QDateTime>
#include <QObject>

#include <nlohmann/json.hpp>

using nlohmann::json;

#include "Project.h"

class TimeEntry : public QObject
{
    Q_OBJECT

public:
    TimeEntry(const QString &id,
              const Project &project,
              const QString &description,
              const QString &userId,
              const QDateTime &start,
              const QDateTime &end,
              QObject *parent = nullptr);
    TimeEntry(const TimeEntry &that);
    TimeEntry(QObject *parent = nullptr);

    QString id() const { return m_id; }
    Project project() const { return m_project; }
    QString description() const { return m_description; }
    QString userId() const { return m_userId; }
    bool running() const { return m_running; }
    QDateTime start() const { return m_start; }
    QDateTime end() const { return m_end; }

    void setProject(const Project &project) { m_project = project; }
    void setEnd(const QDateTime &end) { m_end = end; }

    bool isValid() const { return m_isValid; }

    TimeEntry &operator=(const TimeEntry &other);

signals:

private:
    QString m_id;
    Project m_project;
    QString m_description;
    QString m_userId;
    bool m_running{false};
    QDateTime m_start;
    QDateTime m_end;

    bool m_isValid{false};
};

#endif // TIMEENTRY_H
