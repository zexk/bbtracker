#pragma once

#include "../common/stats.h"

namespace bb {

using StatsFn = bool (*)(GameStats& out);

void start_overlay(const char* game_label, StatsFn stats_fn);

} // namespace bb
