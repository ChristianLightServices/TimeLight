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

	bool hasRunningTimeEntry() const { return m_manager->userHasRunningTimeEntry(m_userId); }
	json getRunningTimeEntry() const;
	void stopCurrentTimeEntry(bool async = false);
	void startTimeEntry(const QString &projectId, bool async = false) const;
	void startTimeEntry(const QString &projectId, const QString &description, bool async = false) const;

signals:

private:
	ClockifyManager *m_manager;

	QString m_userId;
	QString m_name;
};

#endif // CLOCKIFYUSER_H
