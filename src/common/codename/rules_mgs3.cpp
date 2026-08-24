#include "rules_mgs3.h"

#include <array>

namespace bb::codename {
namespace {

constexpr TierMask kVEE = kVe | kE;
constexpr TierMask kH_ = kH;

constexpr Cond kFoxhound[] = {
    {StatId::SpecialItemUsed, Op::Eq, 0}, {StatId::Alerts, Op::Eq, 0},
    {StatId::Kills, Op::Eq, 0},           {StatId::SevereInjuries, Op::Lt, 20},
    {StatId::DamageBars, Op::Lt, 5},      {StatId::LifeMedUsed, Op::Eq, 0},
    {StatId::PlayTimeHours, Op::Lt, 5},   {StatId::Continues, Op::Eq, 0},
    {StatId::Saves, Op::Lt, 25},
};

constexpr Cond kStrictRow[] = {
    {StatId::SpecialItemUsed, Op::Eq, 0}, {StatId::Alerts, Op::Eq, 0},
    {StatId::Kills, Op::Eq, 0},           {StatId::SevereInjuries, Op::Lt, 20},
    {StatId::DamageBars, Op::Lt, 5},      {StatId::LifeMedUsed, Op::Eq, 0},
    {StatId::PlayTimeHours, Op::Lt, 5},   {StatId::Continues, Op::Eq, 0},
    {StatId::Saves, Op::Lt, 25},
};

constexpr Cond kFoxExtreme[] = {
    {StatId::SpecialItemUsed, Op::Eq, 0}, {StatId::Alerts, Op::Le, 3},
    {StatId::Kills, Op::Eq, 0},           {StatId::LifeMedUsed, Op::Eq, 0},
    {StatId::PlayTimeHours, Op::Lt, 5},   {StatId::Continues, Op::Eq, 0},
    {StatId::Saves, Op::Lt, 35},
};

constexpr Cond kMidTier[] = {
    {StatId::SpecialItemUsed, Op::Eq, 0}, {StatId::Alerts, Op::Le, 3},
    {StatId::Kills, Op::Eq, 0},           {StatId::LifeMedUsed, Op::Eq, 0},
    {StatId::PlayTimeHours, Op::Lt, 5},   {StatId::Continues, Op::Eq, 0},
    {StatId::Saves, Op::Lt, 35},
};

constexpr Cond kHighTier[] = {
    {StatId::SpecialItemUsed, Op::Eq, 0}, {StatId::Alerts, Op::Le, 5},
    {StatId::Kills, Op::Eq, 0},           {StatId::LifeMedUsed, Op::Eq, 0},
    {StatId::PlayTimeHours, Op::Lt, 5.5}, {StatId::Continues, Op::Eq, 0},
};

constexpr Cond kTopExtreme[] = {
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

constexpr TierMask kAnyTier = kAllTiers;

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
    RankRule{"FOXHOUND", kX, Kind::Elite, kFoxhound},

    RankRule{"FOX", kH_, Kind::Elite, kStrictRow},
    RankRule{"FOX", kX, Kind::Elite, kFoxExtreme},

    RankRule{"DOBERMAN", kN, Kind::Elite, kStrictRow},
    RankRule{"DOBERMAN", kH_, Kind::Elite, kMidTier},
    RankRule{"DOBERMAN", kX, Kind::Elite, kHighTier},

    RankRule{"HOUND", kE, Kind::Elite, kStrictRow},
    RankRule{"HOUND", kN, Kind::Elite, kMidTier},
    RankRule{"HOUND", kH_, Kind::Elite, kHighTier},
    RankRule{"HOUND", kX, Kind::Elite, kTopExtreme},

    RankRule{"Chicken", kVEE, Kind::Worst, kWorst},
    RankRule{"Mouse", kN, Kind::Worst, kWorst},
    RankRule{"Rabbit", kH_, Kind::Worst, kWorst},
    RankRule{"Ostrich", kX, Kind::Worst, kWorst},

    RankRule{"Kerotan", kAllTiers, Kind::Special, kKerotan},
    RankRule{"Markhor", kAllTiers, Kind::Special, kMarkhor},
    RankRule{"Tsuchinoko", kAllTiers, Kind::Special, kTsuchinoko},
    RankRule{"Chameleon", kAllTiers, Kind::Special, kChameleon},
    RankRule{"Leech", kAllTiers, Kind::Special, kLeech},
    RankRule{"Pigeon", kAllTiers, Kind::Special, kPigeon},

    RankRule{"Night Owl", kX, Kind::Special, kLowInjury},
    RankRule{"Flying Fox", kH_, Kind::Special, kLowInjury},
    RankRule{"Bat", kN, Kind::Special, kLowInjury},
    RankRule{"Flying Squirrel", kVEE, Kind::Special, kLowInjury},

    RankRule{"Eagle", kX, Kind::Special, kFast},
    RankRule{"Hawk", kH_, Kind::Special, kFast},
    RankRule{"Falcon", kN, Kind::Special, kFast},
    RankRule{"Swallow", kVEE, Kind::Special, kFast},

    RankRule{"Whale", kX, Kind::Special, kManyMeals},
    RankRule{"Mammoth", kH_, Kind::Special, kManyMeals},
    RankRule{"Elephant", kN, Kind::Special, kManyMeals},
    RankRule{"Pig", kVEE, Kind::Special, kManyMeals},

    RankRule{"Cow", kAllTiers, Kind::Special, kCow},

    RankRule{"Orca", kX, Kind::Special, kManyKills},
    RankRule{"Jaws", kH_, Kind::Special, kManyKills},
    RankRule{"Shark", kN, Kind::Special, kManyKills},
    RankRule{"Piranha", kVEE, Kind::Special, kManyKills},

    RankRule{"Tasmanian Devil", kX, Kind::Special, kManyInjury},
    RankRule{"Jackal", kH_, Kind::Special, kManyInjury},
    RankRule{"Hyena", kN, Kind::Special, kManyInjury},
    RankRule{"Mongoose", kVEE, Kind::Special, kManyInjury},

    RankRule{"Giant Panda", kX, Kind::Special, kLongTime},
    RankRule{"Sloth", kH_, Kind::Special, kLongTime},
    RankRule{"Capybara", kN, Kind::Special, kLongTime},
    RankRule{"Koala", kVEE, Kind::Special, kLongTime},

    RankRule{"Hippopotamus", kX, Kind::Special, kManySaves},
    RankRule{"Zebra", kH_, Kind::Special, kManySaves},
    RankRule{"Deer", kN, Kind::Special, kManySaves},
    RankRule{"Cat", kVEE, Kind::Special, kManySaves},

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

}
