#ifndef UTILS_H
#define UTILS_H

#include <QGridLayout>

namespace TimeLight
{
    constexpr auto appRestartCode{0x1337beef};
    void restartApp();

    void addVerticalStretchToQGridLayout(QGridLayout *layout);

    const auto teamsAppId{QStringLiteral("738d4410-7778-40b4-8605-0da33ea672ad")};
} // namespace TimeLight

#endif // UTILS_H
