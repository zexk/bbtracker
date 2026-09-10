#pragma once

#include <span>

#include "codename.h"

namespace bb::codename {

std::span<const RankRule> mgs1_rules();
std::span<const RankRule> mgs1_integral_rules();

// The elite ladder, best-first with its display labels: radar, alerts,
// kills, rations, continues, play time. Integral drops the radar row and
// matches on the rest, so both editions share this one table.
std::span<const ReqRow> mgs1_elite_rows();
std::span<const Cond> mgs1_elite_conds();
const char* mgs1_regular_name(const GameStats& s);

}
