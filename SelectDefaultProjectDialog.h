#ifndef SELECTDEFAULTPROJECTDIALOG_H
#define SELECTDEFAULTPROJECTDIALOG_H

#include <QDialog>
#include <QRadioButton>
#include <QPushButton>
#include <QButtonGroup>
#include <QLineEdit>

class SelectDefaultProjectDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SelectDefaultProjectDialog(bool useLastProject,
										bool useLastDescription,
										QString oldDefaultProject,
										QString oldDefaultDescription,
										QPair<QStringList, QStringList> availableProjects,
										QWidget *parent = nullptr);

	bool useLastProject() const { return m_useLastProject; }
	bool useLastDescription() const { return m_useLastDescription; }
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

	QPushButton *m_selectDefaultProject;
	QLineEdit *m_defaultDescriptionEdit;

	bool m_useLastProject;
	bool m_useLastDescription;
	QString m_defaultProject;
	QString m_defaultDescription;
	QPair<QStringList, QStringList> m_availableProjects;
};

#endif // SELECTDEFAULTPROJECTDIALOG_H
