#ifndef CLOCKIFYWORKSPACE_H
#define CLOCKIFYWORKSPACE_H

#include <QObject>

class ClockifyWorkspace : public QObject
{
	Q_OBJECT

public:
	explicit ClockifyWorkspace(const QString &id, const QString &name, QObject *parent = nullptr);
	explicit ClockifyWorkspace(const ClockifyWorkspace &other);

	QString id() const { return m_id; }
	QString name() const { return m_name; }

private:
	QString m_id;
	QString m_name;
};

#endif // CLOCKIFYWORKSPACE_H
