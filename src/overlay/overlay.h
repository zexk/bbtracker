#pragma once

#include "../common/stats.h"

namespace bb {

enum class Game : int {
    MGS2,
    MGS3,
};

using StatsFn = bool (*)(GameStats& out);

void start_overlay(const char* game_label, StatsFn stats_fn, const wchar_t* game_module, Game game);

} // namespace bb
