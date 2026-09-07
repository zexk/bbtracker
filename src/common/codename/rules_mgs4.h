#pragma once

#include <span>

#include "codename.h"

namespace bb::codename {

std::span<const RankRule> mgs4_rules();

// The BIG BOSS ladder backing both matching and the requirements panel.
std::span<const ReqRow> mgs4_elite_rows();

} // namespace bb::codename
