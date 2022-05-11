#include "Utils.h"

#include <QWidget>

void TimeLight::restartApp()
{
    qApp->exit(appRestartCode);
}

void TimeLight::addVerticalStretchToQGridLayout(QGridLayout *layout)
{
    layout->addWidget(new QWidget{layout->parentWidget()}, layout->rowCount(), 0, 1, layout->columnCount());
    layout->setRowStretch(layout->rowCount() - 1, 1);
}
