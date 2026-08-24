#pragma once

#include "../common/stats.h"

namespace bb {

enum class Game : int {
    MG1,
    MG2,
    MGS1,
    MGS2,
    MGS3,
};

using StatsFn = bool (*)(GameStats& out);

void start_overlay(const char* game_label, StatsFn stats_fn, const wchar_t* game_module, Game game);

} // namespace bb
