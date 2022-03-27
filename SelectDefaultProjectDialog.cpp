#include "SelectDefaultProjectDialog.h"

#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QDialogButtonBox>

SelectDefaultProjectDialog::SelectDefaultProjectDialog(bool useLastProject,
                                                       bool useLastDescription,
                                                       bool disableDescription,
                                                       const QString &oldDefaultProject,
                                                       const QString &oldDefaultDescription,
                                                       const QPair<QStringList, QStringList> &availableProjects,
                                                       QWidget *parent)
    : QDialog{parent},
      m_projectButtons{new QButtonGroup{this}},
      m_useLastProjectBtn{new QRadioButton{tr("Use the project from the last task"), this}},
      m_useSpecificProjectBtn{new QRadioButton{tr("Use a specific project every time"), this}},
      m_descriptionButtons{new QButtonGroup{this}},
      m_useLastDescriptionBtn{new QRadioButton{tr("Use the description from the last task"), this}},
      m_useSpecificDescriptionBtn{new QRadioButton{tr("Use a specific description every time"), this}},
      m_useNoDescriptionBtn{new QRadioButton{tr("Don't use a description"), this}},
      m_defaultProjectCombo{new QComboBox{this}},
      m_defaultDescriptionEdit{new QLineEdit{oldDefaultDescription, this}},
      m_useLastProject{useLastProject},
      m_useLastDescription{useLastDescription},
      m_disableDescription{disableDescription},
      m_defaultProject{(oldDefaultProject.isEmpty() ? availableProjects.first.first() : oldDefaultProject)},
      m_availableProjects{availableProjects}
{
    auto buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this};

    auto layout	= new QVBoxLayout{this};

	auto projectGroup = new QGroupBox{tr("Default project settings"), this};
	auto descriptionGroup = new QGroupBox{tr("Default description settings"), this};

	auto projectGroupLayout = new QGridLayout{projectGroup};

	m_projectButtons->addButton(m_useLastProjectBtn);
	m_projectButtons->addButton(m_useSpecificProjectBtn);

	if (m_useLastProject)
	{
		m_useLastProjectBtn->setChecked(true);
		m_defaultProjectCombo->setEnabled(false);
	}
	else
		m_useSpecificProjectBtn->setChecked(true);

	m_defaultProjectCombo->addItems(m_availableProjects.second);
	m_defaultProjectCombo->setCurrentIndex(m_availableProjects.first.indexOf(m_defaultProject));

	projectGroupLayout->addWidget(m_useSpecificProjectBtn, 0, 0);
	projectGroupLayout->addWidget(m_defaultProjectCombo, 0, 1);
	projectGroupLayout->addWidget(m_useLastProjectBtn, 1, 0);

	projectGroup->setLayout(projectGroupLayout);

	auto descriptionGroupLayout = new QGridLayout{descriptionGroup};

	m_descriptionButtons->addButton(m_useLastDescriptionBtn);
	m_descriptionButtons->addButton(m_useSpecificDescriptionBtn);
	m_descriptionButtons->addButton(m_useNoDescriptionBtn);

	if (m_disableDescription)
	{
		m_useNoDescriptionBtn->setChecked(true);
		m_defaultDescriptionEdit->setEnabled(false);
	}
	else if (m_useLastDescription)
	{
		m_useLastDescriptionBtn->setChecked(true);
		m_defaultDescriptionEdit->setEnabled(false);
	}
	else
		m_useSpecificDescriptionBtn->setChecked(true);

	descriptionGroupLayout->addWidget(m_useSpecificDescriptionBtn, 0, 0);
	descriptionGroupLayout->addWidget(m_defaultDescriptionEdit, 0, 1);
	descriptionGroupLayout->addWidget(m_useLastDescriptionBtn, 1, 0);
	descriptionGroupLayout->addWidget(m_useNoDescriptionBtn, 2, 0);

	descriptionGroup->setLayout(descriptionGroupLayout);

	layout->addWidget(projectGroup);
	layout->addWidget(descriptionGroup);
	layout->addWidget(buttons);

	connect(m_projectButtons, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this, [this](QAbstractButton *) {
		if (m_projectButtons->checkedButton() == static_cast<QAbstractButton *>(m_useSpecificProjectBtn))
		{
			m_useLastProject = false;
			m_defaultProjectCombo->setEnabled(true);
		}
		else
		{
			if (m_projectButtons->checkedButton() == static_cast<QAbstractButton *>(m_useLastProjectBtn))
				m_useLastProject = true;

			m_defaultProjectCombo->setEnabled(false);
		}
	});

	connect(m_defaultProjectCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
		m_defaultProject = m_availableProjects.first[m_defaultProjectCombo->currentIndex()];
	});

	connect(m_descriptionButtons, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this, [this](QAbstractButton *) {
		if (m_descriptionButtons->checkedButton() == static_cast<QAbstractButton *>(m_useSpecificDescriptionBtn))
		{
			m_useLastDescription = false;
			m_disableDescription = false;
			m_defaultDescriptionEdit->setEnabled(true);
		}
		else
		{
			if (m_descriptionButtons->checkedButton() == static_cast<QAbstractButton *>(m_useLastDescriptionBtn))
			{
				m_disableDescription = false;
				m_useLastDescription = true;
			}
			else if (m_descriptionButtons->checkedButton() == static_cast<QAbstractButton *>(m_useNoDescriptionBtn))
			{
				m_useLastDescription = false;
				m_disableDescription = true;
			}

			m_defaultDescriptionEdit->setEnabled(false);
		}
	});

	connect(m_defaultDescriptionEdit, &QLineEdit::textChanged, this, [this]() {
		m_defaultDescription = m_defaultDescriptionEdit->text();
	});

	connect(buttons, &QDialogButtonBox::accepted, this, &SelectDefaultProjectDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &SelectDefaultProjectDialog::reject);
}
