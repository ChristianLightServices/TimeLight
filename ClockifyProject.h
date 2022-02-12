#ifndef CLOCKIFYPROJECT_H
#define CLOCKIFYPROJECT_H

#include <QObject>

#include <nlohmann/json.hpp>

using nlohmann::json;

class ClockifyProject : public QObject
{
	Q_OBJECT
	
public:
	ClockifyProject(const QString &id, const QString &name, const QString &description = QString{}, QObject *parent = nullptr);
	ClockifyProject(const ClockifyProject &that);
	ClockifyProject();

	QString id() const { return m_id; }
	QString name() const { return m_name; }
	QString description() const { return m_description; }

	ClockifyProject &operator=(const ClockifyProject &other);
	
private:
	QString m_id;
	QString m_name;
	QString m_description;
};

#endif // CLOCKIFYPROJECT_H
