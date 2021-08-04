#ifndef TRAYICONS_H
#define TRAYICONS_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QTimer>

#include "ClockifyManager.h"
#include "ClockifyProject.h"
#include "ClockifyUser.h"

class TrayIcons : public QObject
{
	Q_OBJECT
public:
	explicit TrayIcons(const QSharedPointer<ClockifyUser> &user, QObject *parent = nullptr);
	~TrayIcons();

	void show();

	ClockifyProject defaultProject() const;

signals:

private slots:
	void updateTrayIcons();
	void getNewProjectId();
	void getNewApiKey();

private:
	void setUpTrayIcons();

	void setClockifyRunningIconTooltip(const QPair<QString, QIcon> &data);
	void setRunningJobIconTooltip(const QPair<QString, QIcon> &data);

	QSystemTrayIcon *m_clockifyRunning{nullptr};
	QSystemTrayIcon *m_runningJob{nullptr};

	// we'll cache icons in these to avoid having to reset icons lots
	const QIcon *m_clockifyRunningCurrentIcon;
	const QIcon *m_runningJobCurrentIcon;

	QSharedPointer<ClockifyUser> m_user;

	QString m_defaultProjectId;
	QString m_apiKey;

	QTimer m_eventLoop;

	static QPair<QString, QIcon> s_clockifyOn;
	static QPair<QString, QIcon> s_clockifyOff;
	static QPair<QString, QIcon> s_onBreak;
	static QPair<QString, QIcon> s_working;
	static QPair<QString, QIcon> s_notWorking;
	static QPair<QString, QIcon> s_powerNotConnected;
	static QPair<QString, QIcon> s_runningNotConnected;
};

#endif // TRAYICONS_H
