#ifndef TRAYICONS_H
#define TRAYICONS_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QTimer>

#include "ClockifyManager.h"
#include "Project.h"
#include "User.h"

class TrayIcons : public QObject
{
	Q_OBJECT

public:
	explicit TrayIcons(QObject *parent = nullptr);
	~TrayIcons();

	void show();

	Project defaultProject();
	void setUser(const User &user);

	bool valid() const { return m_valid; }

signals:

public slots:
	void setEventLoopInterval(int interval);

private slots:
	void updateTrayIcons();
	void getNewProjectId();
	void getNewApiKey();
	void getNewWorkspaceId();
	void getNewBreakTimeId();

	void showAboutDialog();
	void showLicenseDialog(QWidget *parent = nullptr);

private:
	void setUpTrayIcons();

	void setClockifyRunningIconTooltip(const QPair<QString, QIcon> &data);
	void setRunningJobIconTooltip(const QPair<QString, QIcon> &data);

	AbstractTimeServiceManager *m_manager;

	QSystemTrayIcon *m_clockifyRunning{nullptr};
	QSystemTrayIcon *m_runningJob{nullptr};

	// we'll cache icons and tooltips in these to avoid having to reset stuff a lot
	const QIcon *m_clockifyRunningCurrentIcon{nullptr};
	const QIcon *m_runningJobCurrentIcon{nullptr};
	QString m_clockifyRunningCurrentTooltip;
	QString m_runningJobCurrentTooltip;

	User m_user;

	QString m_apiKey;

	bool m_useLastProject;
	bool m_useLastDescription;
	bool m_disableDescription;
	QString m_defaultProjectId;
	QString m_defaultDescription;
	QString m_breakTimeId;

	QTimer m_eventLoop;
	int m_eventLoopInterval{};

	bool m_valid{true};

	static QPair<QString, QIcon> s_clockifyOn;
	static QPair<QString, QIcon> s_clockifyOff;
	static QPair<QString, QIcon> s_onBreak;
	static QPair<QString, QIcon> s_working;
	static QPair<QString, QIcon> s_notWorking;
	static QPair<QString, QIcon> s_powerNotConnected;
	static QPair<QString, QIcon> s_runningNotConnected;
};

#endif // TRAYICONS_H
