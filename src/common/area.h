#pragma once

#include <cstdio>
#include <cstring>

#include "log.h"
#include "stats.h"

namespace bb {

// Records the area the run is in and logs each transition. Every probe reads a
// short stage string on each poll, sanitises it its own way, then wants the
// same thing done with the result: truncate it into GameStats::area_code and
// say something only when it changed.
//
// The latch is function-local, and each game ships as its own ASI, so one
// probe's areas never collide with another's.
inline void set_area(GameStats& out, const char* area)
{
    std::snprintf(out.area_code, sizeof(out.area_code), "%s", area);
    static char last[sizeof(GameStats::area_code)]{};
    if (std::strcmp(out.area_code, last) != 0) {
        LOG_INFO("area: %s", out.area_code);
        std::memcpy(last, out.area_code, sizeof(last));
    }
}

} // namespace bb
