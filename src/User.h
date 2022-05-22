#ifndef USER_H
#define USER_H

#include <QObject>

#include <optional>

#include "AbstractTimeServiceManager.h"
#include "TimeEntry.h"

class User : public QObject
{
    Q_OBJECT

public:
    explicit User(const QString &userId,
                  const QString &name,
                  const QString &workspaceId,
                  AbstractTimeServiceManager *parent = nullptr);
    User(const User &that);
    User(QObject *parent = nullptr);

    bool hasRunningTimeEntry();
    TimeEntry getRunningTimeEntry();
    QDateTime stopCurrentTimeEntry(bool async = false) const;
    void startTimeEntry(const QString &projectId, bool async = false) const;
    void startTimeEntry(const QString &projectId, const QString &description, bool async = false) const;
    void startTimeEntry(const QString &projectId, QDateTime start, bool async = false) const;
    void startTimeEntry(const QString &projectId, const QString &description, QDateTime start, bool async = false) const;
    QVector<TimeEntry> getTimeEntries(std::optional<int> pageNumber = std::nullopt,
                                      std::optional<int> pageSize = std::nullopt);

    User &operator=(const User &other);

    QString userId() const { return m_userId; }
    QString name() const { return m_name; }
    QString workspaceId() const { return m_workspaceId; }
    bool isValid() const { return m_isValid; }

signals:

private:
    QString m_userId;
    QString m_workspaceId;
    QString m_name;

    bool m_isValid{true};

    AbstractTimeServiceManager *m_manager{nullptr};
};

#endif // USER_H
