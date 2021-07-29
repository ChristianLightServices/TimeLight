#ifndef CLOCKIFYUSER_H
#define CLOCKIFYUSER_H

#include <QObject>

#include "ClockifyManager.h"
#include "TimeEntry.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ClockifyUser : public QObject
{
	Q_OBJECT

public:
	explicit ClockifyUser(QString userId, ClockifyManager *manager);

	bool hasRunningTimeEntry();
	TimeEntry getRunningTimeEntry();
	QDateTime stopCurrentTimeEntry(bool async = false) const;
	void startTimeEntry(const QString &projectId, bool async = false) const;
	void startTimeEntry(const QString &projectId, const QString &description, bool async = false) const;
	void startTimeEntry(const QString &projectId, QDateTime start, bool async = false) const;
	void startTimeEntry(const QString &projectId, const QString &description, QDateTime start, bool async = false) const;
	QVector<TimeEntry> getTimeEntries();

signals:

private:
	ClockifyManager *m_manager;

	QString m_userId;
	QString m_name;
};

#endif // CLOCKIFYUSER_H
