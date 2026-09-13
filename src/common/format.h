#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace bb {

inline void format_time(double seconds, char* out, size_t size)
{
    constexpr int kSecondsPerMinute = 60;
    constexpr int kSecondsPerHour = kSecondsPerMinute * kSecondsPerMinute;
    const int total = static_cast<int>(seconds);
    std::snprintf(out, size, "%d:%02d:%02d", total / kSecondsPerHour,
                  (total / kSecondsPerMinute) % kSecondsPerMinute,
                  total % kSecondsPerMinute);
}

inline void format_count(int64_t value, char* out, size_t size)
{
    std::string text = std::to_string(value);
    for (int pos = static_cast<int>(text.size()) - 3; pos > (value < 0 ? 1 : 0); pos -= 3)
        text.insert(pos, ",");
    std::snprintf(out, size, "%s", text.c_str());
}

} // namespace bb
