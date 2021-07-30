#ifndef TIMEENTRY_H
#define TIMEENTRY_H

#include <QObject>
#include <QDateTime>

#include <nlohmann/json.hpp>

using nlohmann::json;

#include "ClockifyProject.h"

class TimeEntry : public QObject
{
	Q_OBJECT

public:
	explicit TimeEntry(json entry, QObject *parent = nullptr);
	TimeEntry();
	explicit TimeEntry(const TimeEntry &that);

	QString id() const { return m_id; }
	ClockifyProject project() const { return m_project; }
	QString userId() const { return m_userId; }
	bool running() const { return m_running; }
	QDateTime start() const { return m_start; }
	QDateTime end() const { return m_end; }

	bool isValid() const { return m_isValid; }

	TimeEntry &operator=(const TimeEntry &other);

signals:

private:
	QString m_id;
	ClockifyProject m_project;
	QString m_userId;
	bool m_running;
	QDateTime m_start;
	QDateTime m_end;

	bool m_isValid{false};
};

#endif // TIMEENTRY_H
