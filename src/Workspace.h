#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <QObject>

class Workspace : public QObject
{
    Q_OBJECT

public:
    explicit Workspace(const QString &id, const QString &name, QObject *parent = nullptr);
    explicit Workspace(const Workspace &that);
    Workspace(QObject *parent = nullptr)
        : QObject{parent}
    {}

    Workspace &operator=(const Workspace &other);

    QString id() const { return m_id; }
    QString name() const { return m_name; }

private:
    QString m_id;
    QString m_name;
};

#endif // WORKSPACE_H
