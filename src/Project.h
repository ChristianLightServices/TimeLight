#ifndef PROJECT_H
#define PROJECT_H

#include <QObject>

#include <nlohmann/json.hpp>

using nlohmann::json;

class Project : public QObject
{
    Q_OBJECT

public:
    Project(const QString &id, const QString &name, const QString &description, QObject *parent = nullptr);
    Project(const QString &id, const QString &name, QObject *parent = nullptr);
    Project(const Project &that);
    Project(QObject *parent = nullptr);

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    QString description() const { return m_description; }

    Project &operator=(const Project &other);

private:
    QString m_id;
    QString m_name;
    QString m_description;
};

#endif // PROJECT_H
