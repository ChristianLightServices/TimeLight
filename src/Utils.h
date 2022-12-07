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
    RangeSlice(IteratorType begin, IteratorType end)
        : m_begin{begin},
          m_end{end}
    {}

    IteratorType begin() noexcept { return m_begin; }
    IteratorType end() noexcept { return m_end; }
    IteratorType cbegin() const noexcept { return m_begin; }
    IteratorType cend() const noexcept { return m_end; }
    std::size_t size() const noexcept { return rangeSize(*this); }

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

    template<class Range>
    std::size_t rangeSize(Range r)
    {
        std::size_t size = 0;
        for (auto it = r.begin(); it != r.end(); ++it, ++size)
            ;
        return size;
    }
} // namespace TimeLight

#endif // UTILS_H
