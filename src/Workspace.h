#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <QObject>

class Workspace : public QObject
{
    Q_OBJECT

public:
    explicit Workspace(const QString &id, const QString &name, QObject *parent = nullptr);
    Workspace(const Workspace &that);
    Workspace(QObject *parent = nullptr)
        : QObject{parent},
          m_valid{false}
    {}

    Workspace &operator=(const Workspace &other);

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    bool valid() const { return m_valid; }

private:
    QString m_id;
    QString m_name;

    bool m_valid{true};
};

#endif // WORKSPACE_H
