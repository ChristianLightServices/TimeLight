#ifndef SELECTDEFAULTPROJECTDIALOG_H
#define SELECTDEFAULTPROJECTDIALOG_H

#include <QDialog>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QComboBox>

class SelectDefaultProjectDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SelectDefaultProjectDialog(bool useLastProject,
										bool useLastDescription,
										bool disableDescription,
										QString oldDefaultProject,
										QString oldDefaultDescription,
										QPair<QStringList, QStringList> availableProjects,
										QWidget *parent = nullptr);

	bool useLastProject() const { return m_useLastProject; }
	bool useLastDescription() const { return m_useLastDescription; }
	bool disableDescription() const { return m_disableDescription; }
	QString selectedProject() const { return m_defaultProject; }
	QString selectedDescription() const { return m_defaultDescription; }

signals:

private:
	QButtonGroup *m_projectButtons;
	QRadioButton *m_useLastProjectBtn;
	QRadioButton *m_useSpecificProjectBtn;

	QButtonGroup *m_descriptionButtons;
	QRadioButton *m_useLastDescriptionBtn;
	QRadioButton *m_useSpecificDescriptionBtn;
	QRadioButton *m_useNoDescriptionBtn;

	QComboBox *m_defaultProjectCombo;
	QLineEdit *m_defaultDescriptionEdit;

	bool m_useLastProject;
	bool m_useLastDescription;
	bool m_disableDescription;
	QString m_defaultProject;
	QString m_defaultDescription;
	QPair<QStringList, QStringList> m_availableProjects;
};

#endif // SELECTDEFAULTPROJECTDIALOG_H
