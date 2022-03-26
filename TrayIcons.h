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
	void timerStateChanged();

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
	enum class TimerState
	{
		Running,
		OnBreak,
		NotRunning,
		Offline,
		StateUnset,
	};

	void setUpTrayIcons();
	void setTimerState(const TimerState state);

	AbstractTimeServiceManager *m_manager;

	QSystemTrayIcon *m_timerRunning{nullptr};
	QSystemTrayIcon *m_runningJob{nullptr};

	User m_user;
	TimerState m_timerState{TimerState::StateUnset};

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

	static QPair<QString, QIcon> s_timerOn;
	static QPair<QString, QIcon> s_timerOff;
	static QPair<QString, QIcon> s_onBreak;
	static QPair<QString, QIcon> s_working;
	static QPair<QString, QIcon> s_notWorking;
	static QPair<QString, QIcon> s_powerNotConnected;
	static QPair<QString, QIcon> s_runningNotConnected;
};

#endif // TRAYICONS_H
