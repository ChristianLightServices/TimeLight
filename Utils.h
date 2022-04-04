#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QApplication>
#include <QGridLayout>

namespace ClockifyTrayIcons
{
    constexpr auto appRestartCode{0x1337beef};
    void restartApp();

    void addVerticalStretchToQGridLayout(QGridLayout *layout);
}

#endif // CONSTANTS_H
