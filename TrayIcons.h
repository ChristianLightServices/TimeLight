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
	~TrayIcons() override;

	void show();

	Project defaultProject();
	void setUser(const User &user);

	bool valid() const { return m_valid; }

signals:
	void timerStateChanged();
	void jobEnded();

public slots:
	void setEventLoopInterval(int interval);

private slots:
	void updateTrayIcons();
	void getNewProjectId();

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
	QString m_currentRunningJobId;

	QTimer m_eventLoop;

	bool m_valid{true};
};

#endif // TRAYICONS_H
