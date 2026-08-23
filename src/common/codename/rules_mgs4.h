#pragma once

#include <span>
#include <vector>

#include "codename.h"

namespace bb::codename {

std::span<const RankRule> mgs4_rules();
std::vector<Match> all_matches_mgs4(const GameStats& s);
std::vector<ReqStatus> elite_requirements_mgs4(const GameStats& s);

}
