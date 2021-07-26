#ifndef CLOCKIFYUSER_H
#define CLOCKIFYUSER_H

#include <QObject>

#include "ClockifyManager.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ClockifyUser : public QObject
{
	Q_OBJECT

public:
	explicit ClockifyUser(QString userId, ClockifyManager *manager);

	bool hasRunningTimeEntry();
	json getRunningTimeEntry();
	QDateTime stopCurrentTimeEntry(bool async = false);
	void startTimeEntry(const QString &projectId, bool async = false) const;
	void startTimeEntry(const QString &projectId, const QString &description, bool async = false) const;
	void startTimeEntry(const QString &projectId, QDateTime start, bool async = false) const;
	void startTimeEntry(const QString &projectId, const QString &description, QDateTime start, bool async = false) const;
	json getTimeEntries();

signals:

private:
	ClockifyManager *m_manager;

	QString m_userId;
	QString m_name;

	bool m_hasRunningTimeEntry;
	json m_runningTimeEntry;

	json m_timeEntries;
};

#endif // CLOCKIFYUSER_H
