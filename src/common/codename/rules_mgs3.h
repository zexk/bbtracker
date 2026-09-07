#pragma once

#include <span>

#include "codename.h"

namespace bb::codename {

std::span<const RankRule> mgs3_rules();

// Step 0 of the elite ladder, backing both matching and the requirements
// panel.
std::span<const ReqRow> mgs3_elite_rows();

}
