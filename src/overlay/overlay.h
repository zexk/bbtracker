#pragma once

#include "../common/stats.h"

namespace bb {

enum class Game : int {
    MG1,
    MG2,
    MGS1,
    MGS2,
    MGS3,
    MGS4,
    MGSPW,
};

using StatsFn = bool (*)(GameStats& out);

// A single fast-moving value the panel redraws every frame, apart from the
// 10 Hz stats poll. Only PW uses one, for its run clock.
using ClockFn = bool (*)(uint32_t& ticks);

void start_overlay(const char* game_label, StatsFn stats_fn, const wchar_t* game_module,
                   Game game, ClockFn clock_fn = nullptr);

} // namespace bb
