#include "DeveloperTools.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QPixmap>
#include <QVBoxLayout>

#include "User.h"
#include "Settings.h"
#include "Utils.h"

DeveloperTools::DeveloperTools(AbstractTimeServiceManager *manager, QWidget *parent)
    : QDialog{parent}
{
    auto user = manager->getApiKeyOwner();

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
        connect(rep, &QNetworkReply::finished, image, [m, rep, image] {
            QPixmap p;
            p.loadFromData(rep->readAll());
            if (p.isNull())
                image->setVisible(false);
            else
                image->setPixmap(p.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m->deleteLater();
            rep->deleteLater();
        });
    }

    auto userId = new QLineEdit{user.userId(), userGroup};
    userId->setReadOnly(true);

    userGroupLayout->addWidget(image, 0, 0, 3, 3);
    userGroupLayout->addWidget(new QLabel{tr("Name:")}, 0, 3);
    userGroupLayout->addWidget(new QLabel{tr("Email:")}, 1, 3);
    userGroupLayout->addWidget(new QLabel{tr("User ID:")}, 2, 3);
    userGroupLayout->addWidget(new QLabel{user.name()}, 0, 4);
    userGroupLayout->addWidget(new QLabel{user.email()}, 1, 4);
    userGroupLayout->addWidget(userId, 2, 4);

    layout->addWidget(userGroup);

    auto buttons = new QDialogButtonBox{this};
    connect(buttons->addButton(tr("Disable developer mode"), QDialogButtonBox::ActionRole), &QPushButton::clicked, this, [this](bool) {
        if (QMessageBox::information(this,
                                  tr("Disable developer mode"),
                                  tr("In order to disable developer mode, the app needs to restart."),
                                  QMessageBox::Ok | QMessageBox::Cancel,
                                  QMessageBox::Ok) == QMessageBox::Ok)
        {
            Settings::instance()->setDeveloperMode(false);
            TimeLight::restartApp();
        }
    });
    buttons->addButton(tr("Done"), QDialogButtonBox::AcceptRole);

    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
}
