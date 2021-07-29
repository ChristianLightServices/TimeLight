#ifndef TIMEENTRY_H
#define TIMEENTRY_H

#include <QObject>
#include <QDateTime>

#include <nlohmann/json.hpp>

using nlohmann::json;

class TimeEntry : public QObject
{
	Q_OBJECT

public:
	explicit TimeEntry(json entry, QObject *parent = nullptr);
	TimeEntry();
	explicit TimeEntry(const TimeEntry &that);

	QString id() const { return m_id; }
	QString projectId() const { return m_projectId; }
	QString description() const { return m_description; }
	QString userId() const { return m_userId; }
	bool running() const { return m_running; }
	QDateTime start() const { return m_start; }
	QDateTime end() const { return m_end; }

	bool isValid() const { return m_isValid; }

	TimeEntry &operator=(const TimeEntry &other);

signals:

private:
	QString m_id;
	QString m_projectId;
	QString m_description;
	QString m_userId;
	bool m_running;
	QDateTime m_start;
	QDateTime m_end;

	bool m_isValid{true};
};

#endif // TIMEENTRY_H
