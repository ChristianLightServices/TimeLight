#ifndef SELECTDEFAULTPROJECTDIALOG_H
#define SELECTDEFAULTPROJECTDIALOG_H

#include <QDialog>
#include <QRadioButton>
#include <QPushButton>
#include <QButtonGroup>

class SelectDefaultProjectDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SelectDefaultProjectDialog(QString oldDefault, QPair<QStringList, QStringList> availableProjects, QWidget *parent = nullptr);

	QString selectedProject() const { return m_defaultProject; }

signals:

private:
	QButtonGroup *m_buttons;
	QRadioButton *m_useLastProject;
	QRadioButton *m_useSpecificProject;

	QPushButton *m_selectDefaultProject;

	QString m_defaultProject;
	QString m_selectedSpecificProject;
	QPair<QStringList, QStringList> m_availableProjects;
};

#endif // SELECTDEFAULTPROJECTDIALOG_H
