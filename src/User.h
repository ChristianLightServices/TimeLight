#ifndef USER_H
#define USER_H

#include <QObject>

#include <optional>

#include "AbstractTimeServiceManager.h"
#include "Project.h"
#include "TimeEntry.h"

class User : public QObject
{
    Q_OBJECT

public:
    explicit User(const QString &userId,
                  const QString &name,
                  const QString &workspaceId,
                  const QString &avatarUrl,
                  const QString &email,
                  QSharedPointer<AbstractTimeServiceManager> parent);
    User(const User &that);
    User(QObject *parent = nullptr);

    std::optional<TimeEntry> getRunningTimeEntry();
    QDateTime stopCurrentTimeEntry(bool async = false) const;
    void startTimeEntry(const Project &project, bool async = false) const;
    void startTimeEntry(const Project &project, QDateTime start, bool async = false) const;
    void modifyTimeEntry(const TimeEntry &t, bool async = false);
    void deleteTimeEntry(const TimeEntry &t, bool async = false);
    QVector<TimeEntry> getTimeEntries(std::optional<int> pageNumber = std::nullopt,
                                      std::optional<int> pageSize = std::nullopt,
                                      std::optional<QDateTime> start = std::nullopt,
                                      std::optional<QDateTime> end = std::nullopt);

    User &operator=(const User &other);

    QSharedPointer<AbstractTimeServiceManager> manager() { return m_manager; }

    QString userId() const { return m_userId; }
    QString name() const { return m_name; }
    QString workspaceId() const { return m_workspaceId; }
    QString avatarUrl() const { return m_avatarUrl; }
    QString email() const { return m_email; }
    bool isValid() const { return m_isValid; }

signals:

private:
    QString m_userId;
    QString m_workspaceId;
    QString m_name;
    QString m_avatarUrl;
    QString m_email;

    bool m_isValid{true};

    QSharedPointer<AbstractTimeServiceManager> m_manager;
};

#endif // USER_H
