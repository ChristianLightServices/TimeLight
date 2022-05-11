#ifndef UTILS_H
#define UTILS_H

#include <QApplication>
#include <QGridLayout>

namespace TimeLight
{
    constexpr auto appRestartCode{0x1337beef};
    void restartApp();

    void addVerticalStretchToQGridLayout(QGridLayout *layout);
} // namespace TimeLight

#endif // UTILS_H
