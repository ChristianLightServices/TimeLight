#ifndef SELECTDEFAULTWORKSPACEDIALOG_H
#define SELECTDEFAULTWORKSPACEDIALOG_H

#include <QObject>

#include "ClockifyWorkspace.h"

class SelectDefaultWorkspaceDialog : public QObject
{
	Q_OBJECT

public:
	SelectDefaultWorkspaceDialog(const QVector<ClockifyWorkspace> &workspaces, QObject *parent = nullptr);

	ClockifyWorkspace selectedWorkspace() const
	{
		return ClockifyWorkspace{m_defaultWorkspaceId, m_workspaceNames[m_workspaceIds.indexOf(m_defaultWorkspaceId)]};
	}
	QString selectedWorkspaceId() const { return m_defaultWorkspaceId; }
	QString selectedWorkspaceName() const { return m_workspaceNames[m_workspaceIds.indexOf(m_defaultWorkspaceId)]; }

	bool getNewWorkspace();

private:
	QStringList m_workspaceIds;
	QStringList m_workspaceNames;

	QString m_defaultWorkspaceId;
};

#endif // SELECTDEFAULTWORKSPACEDIALOG_H
