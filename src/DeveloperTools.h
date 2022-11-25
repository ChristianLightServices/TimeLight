#ifndef DEVELOPERTOOLS_H
#define DEVELOPERTOOLS_H

#include <QDialog>

#include "AbstractTimeServiceManager.h"

class DeveloperTools : public QDialog
{
    Q_OBJECT

public:
    explicit DeveloperTools(QSharedPointer<AbstractTimeServiceManager> manager, QWidget *parent = nullptr);
};

#endif // DEVELOPERTOOLS_H
