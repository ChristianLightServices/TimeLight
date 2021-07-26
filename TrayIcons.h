#ifndef TRAYICONS_H
#define TRAYICONS_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QTimer>

#include "ClockifyManager.h"
#include "ClockifyUser.h"

class TrayIcons : public QObject
{
	Q_OBJECT
public:
	explicit TrayIcons(const QSharedPointer<ClockifyManager> &manager, const QSharedPointer<ClockifyUser> &user, QObject *parent = nullptr);
	~TrayIcons();

	void show();

	QString projectId() const;

signals:

private slots:
	void updateTrayIcons();
	void getNewProjectId();
	void getNewApiKey();

private:
	void setUpTrayIcons();

	QSystemTrayIcon *m_clockifyRunning;
	QSystemTrayIcon *m_runningJob;

	QSharedPointer<ClockifyManager> m_manager;
	QSharedPointer<ClockifyUser> m_user;

	QString m_defaultProjectId;
	QString m_apiKey;

	QTimer m_eventLoop;
};

#endif // TRAYICONS_H
