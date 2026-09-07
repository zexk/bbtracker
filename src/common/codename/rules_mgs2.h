#pragma once

#include <span>

#include "codename.h"

namespace bb::codename {

std::span<const RankRule> mgs2_rules();

// The BIG BOSS ladder backing both strictness-0 matching and the
// requirements panel.
std::span<const ReqRow> mgs2_elite_rows();

}
