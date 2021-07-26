#include "SelectDefaultProjectDialog.h"

#include <QGridLayout>
#include <QLabel>
#include <QInputDialog>
#include <QHBoxLayout>

SelectDefaultProjectDialog::SelectDefaultProjectDialog(QString oldDefault, QPair<QStringList, QStringList> availableProjects, QWidget *parent)
	: QDialog{parent},
	  m_buttons{new QButtonGroup{this}},
	  m_useLastProject{new QRadioButton{"Use the project from the last task", this}},
	  m_useSpecificProject{new QRadioButton{"Use a specific project every time", this}},
	  m_selectDefaultProject{new QPushButton{"Select default project", this}},
	  m_defaultProject{oldDefault},
	  m_selectedSpecificProject{m_defaultProject},
	  m_availableProjects{availableProjects}
{
	auto layout	= new QGridLayout;

	m_buttons->addButton(m_useLastProject);
	m_buttons->addButton(m_useSpecificProject);

	if (m_defaultProject.isEmpty())
		m_useLastProject->setChecked(true);
	else
		m_useSpecificProject->setChecked(true);

	layout->addWidget(m_useSpecificProject, 0, 0);
	layout->addWidget(m_useLastProject, 1, 0);
	layout->addWidget(m_selectDefaultProject, 0, 1);

	auto ok = new QPushButton{"OK", this};
	auto cancel = new QPushButton{"Cancel", this};

	auto buttonLayout = new QHBoxLayout;

	buttonLayout->addStretch();
	buttonLayout->addWidget(ok);
	buttonLayout->addWidget(cancel);

	layout->addLayout(buttonLayout, 2, 0, 2, 1);

	setLayout(layout);

	connect(m_buttons, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this, [this](QAbstractButton *) {
		if (m_buttons->checkedButton() == m_useSpecificProject)
		{
			m_defaultProject = m_selectedSpecificProject;
			m_selectDefaultProject->setEnabled(true);
		}
		else
		{
			if (m_buttons->checkedButton() == m_useLastProject)
				m_defaultProject = "last-entered-code";

			m_selectDefaultProject->setEnabled(false);
		}
	});

	connect(m_selectDefaultProject, &QPushButton::clicked, this, [this]() {
		bool ok{true};
		auto projectName = QInputDialog::getItem(nullptr,
												 "Default project",
												 "Select your default project",
												 m_availableProjects.second,
												 m_availableProjects.first.indexOf(m_defaultProject),
												 false,
												 &ok);
		if (!ok)
			return;
		else
			m_defaultProject = m_selectedSpecificProject = m_availableProjects.first[m_availableProjects.second.indexOf(projectName)];
	});

	connect(ok, &QPushButton::clicked, this, &SelectDefaultProjectDialog::accept);
	connect(cancel, &QPushButton::clicked, this, &SelectDefaultProjectDialog::reject);
}
