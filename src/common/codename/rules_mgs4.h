#pragma once

#include <span>

#include "codename.h"

namespace bb::codename {

std::span<const RankRule> mgs4_rules();

// The BIG BOSS ladder backing both matching and the requirements panel.
std::span<const ReqRow> mgs4_elite_rows();

// Feat thresholds, shared by the rank rules and the overlay's feat panel
// so the displayed goals cannot drift from what the ranks match on.
// Counts are plain counts; the timed feats name their unit.
namespace mgs4_goals {
inline constexpr int kBearChokes = 100;
inline constexpr int kEagleHeadshots = 150;
inline constexpr int kAssassinKnife = 50;
inline constexpr int kAssassinCqcHolds = 50;
inline constexpr int kAssassinMaxAlerts = 25;
inline constexpr int kBlueBirdItems = 50;
inline constexpr int kHawkPraises = 25;
inline constexpr int kLittleGrayWeapons = 69;
inline constexpr int kAntSearches = 50;
inline constexpr int kGibbonHoldUps = 50;
inline constexpr int kTortoiseBoxMinutes = 60;
inline constexpr int kRabbitPages = 100;
inline constexpr int kBeeSyringeUses = 50;
inline constexpr int kGeckoWallMinutes = 60;
inline constexpr int kScarabSideRolls = 100;
inline constexpr int kFrogForwardRolls = 200;
inline constexpr int kInchWormCrawlMinutes = 60;
inline constexpr int kLobsterCrouchMinutes = 150;
inline constexpr int kHyenaPickups = 400;
inline constexpr int kHogCombatHighs = 10;
inline constexpr int kPigRations = 40;
inline constexpr int kCowAlerts = 100;
inline constexpr int kCrocodileKills = 400;
inline constexpr int kGiantPandaHours = 30;
} // namespace mgs4_goals

} // namespace bb::codename
