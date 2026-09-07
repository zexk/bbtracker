#include "rules_mgs3.h"

#include <array>

namespace bb::codename {
namespace {

constexpr TierMask kVEE = kVe | kE;
constexpr TierMask kH_ = kH;
constexpr TierMask kExtreme = kX | (1u << 5);
constexpr TierMask kAllMgs3 = kAllTiers | (1u << 5);

// The elite ladder runs on a diagonal: FOXHOUND sits at Extreme, FOX at Hard,
// DOBERMAN at Normal, HOUND at Easy. Each rank clears step 0 at its own floor
// difficulty and one step looser for every difficulty above that, so these four
// bars serve all four ranks between them. Step 0 doubles as the requirements
// panel rows, so it lives here once as ReqRows and the rules match on the
// derived conds.
constexpr std::array<ReqRow, 9> kEliteStep0Rows{{
    {"special items", StatId::SpecialItemUsed, Op::Eq, 0, ReqFmt::Count},
    {"alerts", StatId::Alerts, Op::Eq, 0, ReqFmt::Count},
    {"kills", StatId::Kills, Op::Eq, 0, ReqFmt::Count},
    {"severe injuries", StatId::SevereInjuries, Op::Lt, 20, ReqFmt::Count},
    {"damage", StatId::DamageBars, Op::Lt, 5, ReqFmt::Bars},
    {"life medicine", StatId::LifeMedUsed, Op::Eq, 0, ReqFmt::Count},
    {"play time", StatId::PlayTimeHours, Op::Lt, 5, ReqFmt::Time},
    {"continues", StatId::Continues, Op::Eq, 0, ReqFmt::Count},
    {"saves", StatId::Saves, Op::Lt, 25, ReqFmt::Count},
}};

constexpr std::array<Cond, 9> kEliteStep0 = conds_from_rows(kEliteStep0Rows);

constexpr Cond kEliteStep1[] = {
    {StatId::SpecialItemUsed, Op::Eq, 0}, {StatId::Alerts, Op::Le, 3},
    {StatId::Kills, Op::Eq, 0},           {StatId::LifeMedUsed, Op::Eq, 0},
    {StatId::PlayTimeHours, Op::Lt, 5},   {StatId::Continues, Op::Eq, 0},
    {StatId::Saves, Op::Lt, 35},
};

constexpr Cond kEliteStep2[] = {
    {StatId::SpecialItemUsed, Op::Eq, 0}, {StatId::Alerts, Op::Le, 5},
    {StatId::Kills, Op::Eq, 0},           {StatId::LifeMedUsed, Op::Eq, 0},
    {StatId::PlayTimeHours, Op::Lt, 5.5}, {StatId::Continues, Op::Eq, 0},
};

constexpr Cond kEliteStep3[] = {
    {StatId::SpecialItemUsed, Op::Eq, 0}, {StatId::Alerts, Op::Le, 10},
    {StatId::Kills, Op::Eq, 0},           {StatId::LifeMedUsed, Op::Eq, 0},
    {StatId::PlayTimeHours, Op::Lt, 6},   {StatId::Continues, Op::Eq, 0},
};

constexpr Cond kWorst[] = {
    {StatId::Alerts, Op::Gt, 250},         {StatId::Kills, Op::Gt, 250},
    {StatId::PlayTimeHours, Op::Gt, 50},   {StatId::Continues, Op::Gt, 60},
    {StatId::Saves, Op::Gt, 100},          {StatId::DamageBars, Op::Gt, 30},
    {StatId::SevereInjuries, Op::Gt, 250}, {StatId::LifeMedUsed, Op::Gt, 10},
};

constexpr Cond kMarkhor[] = {{StatId::PlantsCaptured, Op::Ge, 48}};
constexpr Cond kKerotan[] = {{StatId::Kerotans, Op::Eq, 64}};
constexpr Cond kLeech[] = {{StatId::LeechAttached, Op::Eq, 1}};
constexpr Cond kTsuchinoko[] = {{StatId::TsuchinokoAlive, Op::Eq, 1}};
constexpr Cond kChameleon[] = {{StatId::Alerts, Op::Eq, 0}};
constexpr Cond kPigeon[] = {{StatId::Kills, Op::Eq, 0}};

constexpr Cond kLowInjury[] = {{StatId::SevereInjuries, Op::Lt, 20}};
constexpr Cond kFast[] = {{StatId::PlayTimeHours, Op::Lt, 5}};
constexpr Cond kManyMeals[] = {{StatId::MealsEaten, Op::Gt, 250}};
constexpr Cond kCow[] = {{StatId::Alerts, Op::Gt, 250}};
constexpr Cond kManyKills[] = {{StatId::Kills, Op::Gt, 250}};
constexpr Cond kManyInjury[] = {{StatId::SevereInjuries, Op::Gt, 250}};
constexpr Cond kLongTime[] = {{StatId::PlayTimeHours, Op::Gt, 50}};
constexpr Cond kManySaves[] = {{StatId::Saves, Op::Gt, 100}};

constexpr TierMask kAnyTier = kAllMgs3;

constexpr Cond kRegK1C50A1[] = {
    {StatId::Continues, Op::Le, 50}, {StatId::Kills, Op::Ge, 1},  {StatId::Kills, Op::Le, 100},
    {StatId::Alerts, Op::Ge, 1},     {StatId::Alerts, Op::Le, 20},
};
constexpr Cond kRegK1C50A2[] = {
    {StatId::Continues, Op::Le, 50}, {StatId::Kills, Op::Ge, 1},  {StatId::Kills, Op::Le, 100},
    {StatId::Alerts, Op::Ge, 21},    {StatId::Alerts, Op::Le, 50},
};
constexpr Cond kRegK1C50A3[] = {
    {StatId::Continues, Op::Le, 50}, {StatId::Kills, Op::Ge, 1}, {StatId::Kills, Op::Le, 100},
    {StatId::Alerts, Op::Ge, 51},
};
constexpr Cond kRegK2C50A1[] = {
    {StatId::Continues, Op::Le, 50}, {StatId::Kills, Op::Ge, 101},
    {StatId::Alerts, Op::Le, 20},
};
constexpr Cond kRegK2C50A2[] = {
    {StatId::Continues, Op::Le, 50}, {StatId::Kills, Op::Ge, 101},
    {StatId::Alerts, Op::Ge, 21},    {StatId::Alerts, Op::Le, 50},
};
constexpr Cond kRegK2C50A3[] = {
    {StatId::Continues, Op::Le, 50}, {StatId::Kills, Op::Ge, 101},
    {StatId::Alerts, Op::Ge, 51},
};
constexpr Cond kRegK1C51A1[] = {
    {StatId::Continues, Op::Ge, 51}, {StatId::Kills, Op::Ge, 1},  {StatId::Kills, Op::Le, 100},
    {StatId::Alerts, Op::Le, 20},
};
constexpr Cond kRegK1C51A2[] = {
    {StatId::Continues, Op::Ge, 51}, {StatId::Kills, Op::Ge, 1},  {StatId::Kills, Op::Le, 100},
    {StatId::Alerts, Op::Ge, 21},    {StatId::Alerts, Op::Le, 50},
};
constexpr Cond kRegKomodo[] = {
    {StatId::Continues, Op::Ge, 51}, {StatId::Kills, Op::Ge, 1},  {StatId::Kills, Op::Le, 100},
    {StatId::Alerts, Op::Ge, 51},
};
constexpr Cond kRegSpider[] = {
    {StatId::Continues, Op::Ge, 51}, {StatId::Kills, Op::Ge, 101},
    {StatId::Alerts, Op::Le, 20},
};
constexpr Cond kRegPuma[] = {
    {StatId::Continues, Op::Ge, 51}, {StatId::Kills, Op::Ge, 101},
    {StatId::Alerts, Op::Ge, 21},    {StatId::Alerts, Op::Le, 50},
};
constexpr Cond kRegAlligator[] = {
    {StatId::Continues, Op::Ge, 51}, {StatId::Kills, Op::Ge, 101},
    {StatId::Alerts, Op::Ge, 51},
};

const std::array<RankRule, 61> kMgs3Rules{{
    RankRule{"FOXHOUND", kExtreme, Kind::Elite, kEliteStep0},

    RankRule{"FOX", kH_, Kind::Elite, kEliteStep0},
    RankRule{"FOX", kExtreme, Kind::Elite, kEliteStep1},

    RankRule{"DOBERMAN", kN, Kind::Elite, kEliteStep0},
    RankRule{"DOBERMAN", kH_, Kind::Elite, kEliteStep1},
    RankRule{"DOBERMAN", kExtreme, Kind::Elite, kEliteStep2},

    RankRule{"HOUND", kE, Kind::Elite, kEliteStep0},
    RankRule{"HOUND", kN, Kind::Elite, kEliteStep1},
    RankRule{"HOUND", kH_, Kind::Elite, kEliteStep2},
    RankRule{"HOUND", kExtreme, Kind::Elite, kEliteStep3},

    RankRule{kAnimalTiers.worst.low, kVEE, Kind::Worst, kWorst},
    RankRule{kAnimalTiers.worst.normal, kN, Kind::Worst, kWorst},
    RankRule{kAnimalTiers.worst.high, kH_, Kind::Worst, kWorst},
    RankRule{kAnimalTiers.worst.top, kExtreme, Kind::Worst, kWorst},

    RankRule{"Kerotan", kAllMgs3, Kind::Special, kKerotan},
    RankRule{"Markhor", kAllMgs3, Kind::Special, kMarkhor},
    RankRule{"Tsuchinoko", kAllMgs3, Kind::Special, kTsuchinoko},
    RankRule{"Chameleon", kAllMgs3, Kind::Special, kChameleon},
    RankRule{"Leech", kAllMgs3, Kind::Special, kLeech},
    RankRule{"Pigeon", kAllMgs3, Kind::Special, kPigeon},

    RankRule{kAnimalTiers.low_alerts.top, kExtreme, Kind::Special, kLowInjury},
    RankRule{kAnimalTiers.low_alerts.high, kH_, Kind::Special, kLowInjury},
    RankRule{kAnimalTiers.low_alerts.normal, kN, Kind::Special, kLowInjury},
    RankRule{kAnimalTiers.low_alerts.low, kVEE, Kind::Special, kLowInjury},

    RankRule{kAnimalTiers.fast.top, kExtreme, Kind::Special, kFast},
    RankRule{kAnimalTiers.fast.high, kH_, Kind::Special, kFast},
    RankRule{kAnimalTiers.fast.normal, kN, Kind::Special, kFast},
    RankRule{kAnimalTiers.fast.low, kVEE, Kind::Special, kFast},

    RankRule{kAnimalTiers.meals.top, kExtreme, Kind::Special, kManyMeals},
    RankRule{kAnimalTiers.meals.high, kH_, Kind::Special, kManyMeals},
    RankRule{kAnimalTiers.meals.normal, kN, Kind::Special, kManyMeals},
    RankRule{kAnimalTiers.meals.low, kVEE, Kind::Special, kManyMeals},

    RankRule{"Cow", kAllMgs3, Kind::Special, kCow},

    RankRule{kAnimalTiers.kills.top, kExtreme, Kind::Special, kManyKills},
    RankRule{kAnimalTiers.kills.high, kH_, Kind::Special, kManyKills},
    RankRule{kAnimalTiers.kills.normal, kN, Kind::Special, kManyKills},
    RankRule{kAnimalTiers.kills.low, kVEE, Kind::Special, kManyKills},

    RankRule{"Tasmanian Devil", kExtreme, Kind::Special, kManyInjury},
    RankRule{"Jackal", kH_, Kind::Special, kManyInjury},
    RankRule{"Hyena", kN, Kind::Special, kManyInjury},
    RankRule{"Mongoose", kVEE, Kind::Special, kManyInjury},

    RankRule{kAnimalTiers.time.top, kExtreme, Kind::Special, kLongTime},
    RankRule{kAnimalTiers.time.high, kH_, Kind::Special, kLongTime},
    RankRule{kAnimalTiers.time.normal, kN, Kind::Special, kLongTime},
    RankRule{kAnimalTiers.time.low, kVEE, Kind::Special, kLongTime},

    RankRule{kAnimalTiers.saves.top, kExtreme, Kind::Special, kManySaves},
    RankRule{kAnimalTiers.saves.high, kH_, Kind::Special, kManySaves},
    RankRule{kAnimalTiers.saves.normal, kN, Kind::Special, kManySaves},
    RankRule{kAnimalTiers.saves.low, kVEE, Kind::Special, kManySaves},

    RankRule{"Scorpion", kAnyTier, Kind::Regular, kRegK1C50A1},
    RankRule{"Jaguar", kAnyTier, Kind::Regular, kRegK1C50A2},
    RankRule{"Iguana", kAnyTier, Kind::Regular, kRegK1C50A3},

    RankRule{"Tarantula", kAnyTier, Kind::Regular, kRegK2C50A1},
    RankRule{"Panther", kAnyTier, Kind::Regular, kRegK2C50A2},
    RankRule{"Crocodile", kAnyTier, Kind::Regular, kRegK2C50A3},

    RankRule{"Centipede", kAnyTier, Kind::Regular, kRegK1C51A1},
    RankRule{"Leopard", kAnyTier, Kind::Regular, kRegK1C51A2},
    RankRule{"Komodo Dragon", kAnyTier, Kind::Regular, kRegKomodo},

    RankRule{"Spider", kAnyTier, Kind::Regular, kRegSpider},
    RankRule{"Puma", kAnyTier, Kind::Regular, kRegPuma},
    RankRule{"Alligator", kAnyTier, Kind::Regular, kRegAlligator},
}};

} // namespace

std::span<const RankRule> mgs3_rules()
{
    return kMgs3Rules;
}

std::span<const ReqRow> mgs3_elite_rows()
{
    return kEliteStep0Rows;
}

}
