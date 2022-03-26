#include "SelectDefaultWorkspaceDialog.h"

#include <QInputDialog>

SelectDefaultWorkspaceDialog::SelectDefaultWorkspaceDialog(const QVector<Workspace> &workspaces, QObject *parent)
	: QObject{parent}
{
	for (const auto &item : workspaces)
	{
		m_workspaceIds.push_back(item.id());
		m_workspaceNames.push_back(item.name());
	}
}

bool SelectDefaultWorkspaceDialog::getNewWorkspace()
{
	auto ok{false};
	auto workspaceId = QInputDialog::getItem(nullptr,
											 "Select default workspace",
											 "Select the workspace you want to track time on:",
											 m_workspaceNames,
											 m_workspaceIds.indexOf(m_defaultWorkspaceId),
											 false,
											 &ok);

	if (ok)
		m_defaultWorkspaceId = m_workspaceIds[m_workspaceNames.indexOf(workspaceId)];

	return ok;
}
