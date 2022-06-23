#include "DeveloperTools.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QTableView>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Settings.h"
#include "User.h"
#include "Utils.h"

DeveloperTools::DeveloperTools(AbstractTimeServiceManager *manager, QWidget *parent)
    : QDialog{parent}
{
    auto user = manager->getApiKeyOwner();
    auto projects = manager->projects();
    auto workspace = manager->currentWorkspace();

    resize(700, 600);

    auto layout = new QVBoxLayout{this};

    auto userGroup = new QGroupBox{tr("User"), this};
    auto userGroupLayout = new QGridLayout{userGroup};

    auto image = new QLabel{userGroup};
    image->setPixmap(QPixmap{});
    if (user.avatarUrl().isEmpty())
        image->setVisible(false);
    else
    {
        auto m = new QNetworkAccessManager;
        auto rep = m->get(QNetworkRequest{user.avatarUrl()});
        connect(rep, &QNetworkReply::finished, image, [m, rep, image, userGroupLayout] {
            QPixmap p;
            p.loadFromData(rep->readAll());
            if (p.isNull())
                image->deleteLater();
            else
            {
                image->setPixmap(p.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                userGroupLayout->addWidget(image, 0, 0, 4, 1);
            }
            m->deleteLater();
            rep->deleteLater();
        });
    }

    auto userId = new QLineEdit{user.userId(), userGroup};
    userId->setReadOnly(true);
    auto apiKey = new QLineEdit{manager->apiKey(), userGroup};
    apiKey->setReadOnly(true);
    userGroupLayout->addWidget(new QLabel{tr("Name:")}, 0, 1);
    userGroupLayout->addWidget(new QLabel{tr("Email:")}, 1, 1);
    userGroupLayout->addWidget(new QLabel{tr("User ID:")}, 2, 1);
    userGroupLayout->addWidget(new QLabel{tr("API key:")}, 3, 1);
    userGroupLayout->addWidget(new QLabel{user.name()}, 0, 2);
    userGroupLayout->addWidget(new QLabel{user.email()}, 1, 2);
    userGroupLayout->addWidget(userId, 2, 2);
    userGroupLayout->addWidget(apiKey, 3, 2);

    auto workspaceGroup = new QGroupBox{tr("Workspace"), this};
    auto workspaceGroupLayout = new QGridLayout{workspaceGroup};

    auto workspaceId = new QLineEdit{workspace.id(), workspaceGroup};
    workspaceId->setReadOnly(true);

    workspaceGroupLayout->addWidget(new QLabel{tr("Name:")}, 0, 0);
    workspaceGroupLayout->addWidget(new QLabel{tr("Workspace ID:")}, 1, 0);
    workspaceGroupLayout->addWidget(new QLabel{workspace.name()}, 0, 1);
    workspaceGroupLayout->addWidget(workspaceId, 1, 1);

    auto projectGroup = new QGroupBox{tr("Projects"), this};
    auto projectGroupLayout = new QGridLayout{projectGroup};

    auto projectTable = new QTableWidget{static_cast<int>(projects.count()), 2, workspaceGroup};
    for (int i = 0; i < projects.size(); ++i)
    {
        projectTable->setItem(i, 0, new QTableWidgetItem{projects[i].name()});
        auto id = new QLineEdit{projects[i].id()};
        id->setReadOnly(true);
        projectTable->setCellWidget(i, 1, id);
    }
    projectTable->setHorizontalHeaderLabels(QStringList{} << tr("Project name") << tr("Project ID"));
    projectTable->setEditTriggers(QTableWidget::NoEditTriggers);
    projectTable->verticalHeader()->setVisible(false);
    projectTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    projectTable->setSortingEnabled(true);
    projectTable->sortItems(0, Qt::AscendingOrder);
    projectTable->setSelectionMode(QTableWidget::NoSelection);

    projectGroupLayout->addWidget(projectTable, 0, 0);

    auto buttons = new QDialogButtonBox{this};
    connect(buttons->addButton(tr("Disable developer mode"), QDialogButtonBox::ActionRole),
            &QPushButton::clicked,
            this,
            [this](bool) {
                Settings::instance()->setDeveloperMode(false);
                close();
            });
    buttons->addButton(tr("Done"), QDialogButtonBox::AcceptRole);

    layout->addWidget(userGroup);
    layout->addWidget(workspaceGroup);
    layout->addWidget(projectGroup);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
}
