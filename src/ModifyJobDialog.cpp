#include "ModifyJobDialog.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>

ModifyJobDialog::ModifyJobDialog(AbstractTimeServiceManager *manager, const TimeEntry &entry, QWidget *parent)
    : QDialog{parent},
      m_manager{manager},
      m_entry{entry},
      m_availableProjects{m_manager->projects()}
{
    auto layout = new QGridLayout{this};

    QComboBox *project{new QComboBox{this}};
    for (auto &p : m_availableProjects)
        project->addItem(p.name(), p.id());
    project->setCurrentIndex(
        m_availableProjects.indexOf(entry.project().id().isEmpty() ? m_availableProjects.first() : entry.project()));
    layout->addWidget(new QLabel{tr("Project")}, 0, 0);
    layout->addWidget(project, 0, 1);

    QLineEdit *description{new QLineEdit{m_entry.project().description(), this}};
    description->setPlaceholderText(tr("Enter a description..."));

    layout->addWidget(new QLabel{tr("Description")}, 1, 0);
    layout->addWidget(description, 1, 1);

    QDateTimeEdit *start{new QDateTimeEdit{entry.start(), this}};

    layout->addWidget(new QLabel{tr("Start")}, 2, 0);
    layout->addWidget(start, 2, 1);

    auto buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this};
    layout->addWidget(buttons, 3, 1, 1, 2);

    connect(project, &QComboBox::currentIndexChanged, this, [this](int i) { m_entry.setProject(m_availableProjects[i]); });
    connect(start, &QDateTimeEdit::dateTimeChanged, this, [this](const QDateTime &dt) { m_entry.setStart(dt); });
    connect(description, &QLineEdit::textChanged, this, [this](const QString &d) {
        auto p = m_entry.project();
        p.setDescription(d);
        m_entry.setProject(p);
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
