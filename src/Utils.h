#ifndef UTILS_H
#define UTILS_H

#include <QGridLayout>

template<class IteratorType>
class RangeSlice
{
public:
    RangeSlice(IteratorType begin, const int size)
        : m_begin{begin},
          m_end{begin + size}
    {}

    IteratorType begin() noexcept { return m_begin; }
    IteratorType end() noexcept { return m_end; }

private:
    IteratorType m_begin, m_end;
};

namespace TimeLight
{
    const auto teamsAppId{QStringLiteral("738d4410-7778-40b4-8605-0da33ea672ad")};

    constexpr auto appRestartCode{0x1337beef};
    void restartApp();

    void addVerticalStretchToQGridLayout(QGridLayout *layout);

    std::tuple<int, int, int> msecsToHoursMinutesSeconds(int msecs);
} // namespace TimeLight

#endif // UTILS_H
