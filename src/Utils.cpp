#include "Utils.h"

#include <QApplication>
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

void TimeLight::addHorizontalStretchToQGridLayout(QGridLayout *layout)
{
    layout->addWidget(new QWidget{layout->parentWidget()}, layout->columnCount(), 0, 1, layout->rowCount());
    layout->setColumnStretch(layout->columnCount() - 1, 1);
}

std::tuple<int, int, int> TimeLight::msecsToHoursMinutesSeconds(int msecs)
{
    int h = msecs / 1000 / 60 / 60;
    msecs -= h * 1000 * 60 * 60;
    int m = msecs / 1000 / 60;
    msecs -= m * 1000 * 60;
    int s = msecs / 1000;

    return {h, m, s};
}
