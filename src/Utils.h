#ifndef UTILS_H
#define UTILS_H

#include <QGridLayout>

namespace TimeLight
{
    const auto teamsAppId{QStringLiteral("738d4410-7778-40b4-8605-0da33ea672ad")};

    constexpr auto appRestartCode{0x1337beef};
    void restartApp();

    void addVerticalStretchToQGridLayout(QGridLayout *layout);

    std::tuple<int, int, int> msecsToHoursMinutesSeconds(int msecs);
} // namespace TimeLight

#endif // UTILS_H
