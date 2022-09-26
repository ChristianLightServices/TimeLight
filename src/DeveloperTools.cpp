#include "DeveloperTools.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QTableView>
#include <QTableWidget>
#include <QVBoxLayout>

#include "Logger.h"
#include "Settings.h"
#include "User.h"

class DownloadableImage : public QAbstractButton
{
    Q_OBJECT

public:
    explicit DownloadableImage(const QPixmap &pixmap, QStringView id, QWidget *parent = nullptr)
        : QAbstractButton{parent},
          m_pixmap{pixmap},
          m_id{id}
    {
        setToolTip(tr("Save image"));
        connect(this, &QAbstractButton::clicked, this, &DownloadableImage::save);
    }

    explicit DownloadableImage(QStringView id, QWidget *parent = nullptr)
        : DownloadableImage{{}, id, parent}
    {}

    void setPixmap(const QPixmap &p)
    {
        m_pixmap = p;
        resize(m_pixmap.size());
        setMinimumSize(m_pixmap.size());
        repaint();
    }

    virtual void paintEvent(QPaintEvent *e) override
    {
        QPainter p{this};
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawPixmap(rect(), m_pixmap);

        if (isDown())
        {
            p.fillRect(rect(), QBrush{QColor{0x00, 0x00, 0x00, 0x77}});
            p.setBrush(QBrush{QColor{0x62, 0x62, 0x62, 0xe7}});
        }
        else if (underMouse())
        {
            p.fillRect(rect(), QBrush{QColor{"#77444444"}});
            p.setBrush(QBrush{QColor{"#e7e9e9e9"}});
        }
        else
            return;

        p.setPen(Qt::PenStyle::NoPen);
        auto side = std::min(rect().width(), rect().height()) / 2 - 25;
        p.drawEllipse(QRect{side, side, 50, 50});
        QSvgRenderer{QStringLiteral(":/icons/download.svg")}.render(
            &p, QRect{rect().width() / 2 - 24, rect().height() / 2 - 24, 48, 48});
    }

public slots:
    void save()
    {
        auto path = QFileDialog::getSaveFileName(this,
                                                 tr("Save image"),
                                                 QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) +
                                                     QStringLiteral("/%1.png").arg(m_id),
                                                 tr("Image files (*.png *.jpg *.bmp)"));
        if (path.isEmpty())
            return;
        if (!m_pixmap.save(path))
            QMessageBox::warning(this, tr("Save error"), tr("Could not save file to %1.").arg(path));
    }

private:
    QPixmap m_pixmap;
    QStringView m_id;
};

// to make the moc happy
#include "DeveloperTools.moc"

DeveloperTools::DeveloperTools(AbstractTimeServiceManager *manager, QWidget *parent)
    : QDialog{parent}
{
    auto user = manager->getApiKeyOwner();
    auto projects = manager->projects();
    auto workspace = manager->currentWorkspace();

    resize(700, 600);

    auto layout = new QVBoxLayout{this};

    auto timeServiceGroup = new QGroupBox{manager->serviceName(), this};
    auto timeServiceLayout = new QGridLayout{timeServiceGroup};

    auto apiBaseUrl = new QLineEdit{manager->apiBaseUrl(), timeServiceGroup};
    apiBaseUrl->setReadOnly(true);

    timeServiceLayout->addWidget(new QLabel{tr("API base URL:"), timeServiceGroup}, 0, 0);
    timeServiceLayout->addWidget(apiBaseUrl, 0, 1);

    auto userGroup = new QGroupBox{tr("User"), this};
    auto userGroupLayout = new QGridLayout{userGroup};

    if (!user.avatarUrl().isEmpty())
    {
        auto image = new DownloadableImage{user.userId(), userGroup};
        auto m = new QNetworkAccessManager;
        auto rep = m->get(QNetworkRequest{user.avatarUrl()});
        connect(rep, &QNetworkReply::finished, image, [m, rep, image, userGroupLayout] {
            QPixmap p;
            p.loadFromData(rep->readAll());
            if (p.isNull())
            {
                TimeLight::logs::network()->debug("Attempted to download null avatar");
                image->deleteLater();
            }
            else
            {
                image->setPixmap(p.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                userGroupLayout->addWidget(image, 0, 0, 5, 1);
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
    userGroupLayout->addWidget(new QLabel{tr("Creation time:")}, 4, 1);
    userGroupLayout->addWidget(new QLabel{user.name()}, 0, 2);
    userGroupLayout->addWidget(new QLabel{user.email()}, 1, 2);
    userGroupLayout->addWidget(userId, 2, 2);
    userGroupLayout->addWidget(apiKey, 3, 2);
    userGroupLayout->addWidget(
        new QLabel{
            QDateTime::fromSecsSinceEpoch(user.userId().mid(0, 8).toLong(nullptr, 16)).toString("MMMM d, yyyy h:mm:ss A")},
        4,
        2);

    auto workspaceGroup = new QGroupBox{tr("Workspace"), this};
    auto workspaceGroupLayout = new QGridLayout{workspaceGroup};

    auto workspaceId = new QLineEdit{workspace.id(), workspaceGroup};
    workspaceId->setReadOnly(true);

    workspaceGroupLayout->addWidget(new QLabel{tr("Name:")}, 0, 0);
    workspaceGroupLayout->addWidget(new QLabel{tr("Workspace ID:")}, 1, 0);
    workspaceGroupLayout->addWidget(new QLabel{tr("Creation time:")}, 2, 0);
    workspaceGroupLayout->addWidget(new QLabel{workspace.name()}, 0, 1);
    workspaceGroupLayout->addWidget(workspaceId, 1, 1);
    workspaceGroupLayout->addWidget(
        new QLabel{
            QDateTime::fromSecsSinceEpoch(workspace.id().mid(0, 8).toLong(nullptr, 16)).toString("MMMM d, yyyy h:mm:ss A")},
        2,
        1);

    auto projectGroup = new QGroupBox{tr("Projects"), this};
    auto projectGroupLayout = new QGridLayout{projectGroup};

    auto projectTable = new QTableWidget{static_cast<int>(projects.count()), 3, workspaceGroup};
    for (int i = 0; i < projects.size(); ++i)
    {
        projectTable->setItem(i, 0, new QTableWidgetItem{projects[i].name()});
        auto id = new QLineEdit{projects[i].id()};
        id->setReadOnly(true);
        projectTable->setCellWidget(i, 1, id);
        projectTable->setItem(i, 1, new QTableWidgetItem{projects[i].id()});
        projectTable->setItem(
            i,
            2,
            new QTableWidgetItem{QDateTime::fromSecsSinceEpoch(projects[i].id().mid(0, 8).toLong(nullptr, 16))
                                     .toString("MMMM d, yyyy h:mm:ss A")});
    }
    projectTable->setHorizontalHeaderLabels(QStringList{} << tr("Project name") << tr("Project ID") << tr("Creation time"));
    projectTable->setEditTriggers(QTableWidget::NoEditTriggers);
    projectTable->verticalHeader()->setVisible(false);
    projectTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    projectTable->setSortingEnabled(true);
    projectTable->sortItems(0, Qt::AscendingOrder);
    projectTable->setSelectionMode(QTableWidget::NoSelection);

    projectGroupLayout->addWidget(projectTable, 0, 0);

    auto buttons = new QDialogButtonBox{this};
    connect(
        buttons->addButton(tr("Disable developer mode"), QDialogButtonBox::ActionRole), &QPushButton::clicked, this, [this] {
            Settings::instance()->setDeveloperMode(false);
            close();
        });
    buttons->addButton(tr("Close"), QDialogButtonBox::AcceptRole);

    layout->addWidget(timeServiceGroup);
    layout->addWidget(userGroup);
    layout->addWidget(workspaceGroup);
    layout->addWidget(projectGroup);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
}
