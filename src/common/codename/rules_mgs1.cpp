#include "rules_mgs1.h"

#include <array>

namespace bb::codename {
namespace {

constexpr TierMask kAny = kVe | kE | kN | kH | kX;

constexpr Cond kFox[] = {
    {StatId::Alerts, Op::Lt, 4},      {StatId::Kills, Op::Lt, 25},
    {StatId::RationsUsed, Op::Le, 1}, {StatId::Continues, Op::Eq, 0},
    {StatId::PlayTimeHours, Op::Lt, 3},
};

constexpr Cond kBigBoss[] = {
    {StatId::RadarOff, Op::Eq, 1},    {StatId::Alerts, Op::Lt, 4},
    {StatId::Kills, Op::Lt, 25},      {StatId::RationsUsed, Op::Le, 1},
    {StatId::Continues, Op::Eq, 0},   {StatId::PlayTimeHours, Op::Lt, 3},
};

constexpr Cond kFalcon[] = {{StatId::PlayTimeHours, Op::Lt, 2.5}};
constexpr Cond kJaws[] = {{StatId::Kills, Op::Gt, 250}};
constexpr Cond kPig[] = {{StatId::RationsUsed, Op::Gt, 120}};
constexpr Cond kHippo[] = {{StatId::Saves, Op::Gt, 80}};
constexpr Cond kTurtle[] = {{StatId::PlayTimeHours, Op::Gt, 18}};
constexpr Cond kChicken[] = {{StatId::RationsUsed, Op::Gt, 120},
                             {StatId::Saves, Op::Gt, 80},
                             {StatId::PlayTimeHours, Op::Gt, 18}};

constexpr Cond kGridL1Y0[] = {{StatId::Alerts, Op::Ge, 1}, {StatId::Alerts, Op::Lt, 30},
                              {StatId::DiscoveryRatio, Op::Lt, 4}};
constexpr Cond kGridL1Y1[] = {{StatId::Alerts, Op::Ge, 1},  {StatId::Alerts, Op::Lt, 30},
                              {StatId::DiscoveryRatio, Op::Ge, 4},
                              {StatId::DiscoveryRatio, Op::Lt, 8}};
constexpr Cond kGridL1Y2[] = {{StatId::Alerts, Op::Ge, 1},
                              {StatId::Alerts, Op::Lt, 30},
                              {StatId::DiscoveryRatio, Op::Ge, 8},
                              {StatId::DiscoveryRatio, Op::Lt, 16}};
constexpr Cond kGridL1Y3[] = {{StatId::Alerts, Op::Ge, 1},
                              {StatId::Alerts, Op::Lt, 30},
                              {StatId::DiscoveryRatio, Op::Ge, 16},
                              {StatId::DiscoveryRatio, Op::Lt, 20}};
constexpr Cond kGridL1Y4[] = {{StatId::Alerts, Op::Ge, 1},
                              {StatId::Alerts, Op::Lt, 30},
                              {StatId::DiscoveryRatio, Op::Ge, 20}};
constexpr Cond kGridL2Y0[] = {{StatId::Alerts, Op::Ge, 30}, {StatId::Alerts, Op::Lt, 55},
                              {StatId::DiscoveryRatio, Op::Lt, 4}};
constexpr Cond kGridL2Y1[] = {{StatId::Alerts, Op::Ge, 30},
                              {StatId::Alerts, Op::Lt, 55},
                              {StatId::DiscoveryRatio, Op::Ge, 4},
                              {StatId::DiscoveryRatio, Op::Lt, 8}};
constexpr Cond kGridL2Y2[] = {{StatId::Alerts, Op::Ge, 30},
                              {StatId::Alerts, Op::Lt, 55},
                              {StatId::DiscoveryRatio, Op::Ge, 8},
                              {StatId::DiscoveryRatio, Op::Lt, 16}};
constexpr Cond kGridL2Y3[] = {{StatId::Alerts, Op::Ge, 30},
                              {StatId::Alerts, Op::Lt, 55},
                              {StatId::DiscoveryRatio, Op::Ge, 16},
                              {StatId::DiscoveryRatio, Op::Lt, 20}};
constexpr Cond kGridL2Y4[] = {{StatId::Alerts, Op::Ge, 30},
                              {StatId::Alerts, Op::Lt, 55},
                              {StatId::DiscoveryRatio, Op::Ge, 20}};
constexpr Cond kGridL3Y0[] = {{StatId::Alerts, Op::Ge, 55}, {StatId::DiscoveryRatio, Op::Lt, 4}};
constexpr Cond kGridL3Y1[] = {{StatId::Alerts, Op::Ge, 55},
                              {StatId::DiscoveryRatio, Op::Ge, 4},
                              {StatId::DiscoveryRatio, Op::Lt, 8}};
constexpr Cond kGridL3Y2[] = {{StatId::Alerts, Op::Ge, 55},
                              {StatId::DiscoveryRatio, Op::Ge, 8},
                              {StatId::DiscoveryRatio, Op::Lt, 16}};
constexpr Cond kGridL3Y3[] = {{StatId::Alerts, Op::Ge, 55},
                              {StatId::DiscoveryRatio, Op::Ge, 16},
                              {StatId::DiscoveryRatio, Op::Lt, 20}};
constexpr Cond kGridL3Y4[] = {{StatId::Alerts, Op::Ge, 55}, {StatId::DiscoveryRatio, Op::Ge, 20}};

const std::array<RankRule, 23> kMgs1Rules{{
    RankRule{"BIG BOSS", kAny, Kind::Elite, kBigBoss},
    RankRule{"FOX", kAny, Kind::Elite, kFox, true},

    RankRule{"Falcon", kAny, Kind::Special, kFalcon, true},
    RankRule{"Jaws", kAny, Kind::Special, kJaws},
    RankRule{"Pig", kAny, Kind::Special, kPig},
    RankRule{"Hippopotamus", kAny, Kind::Special, kHippo},
    RankRule{"Turtle", kAny, Kind::Special, kTurtle, true},
    RankRule{"Chicken", kAny, Kind::Special, kChicken, true},

    RankRule{"Leopard", kAny, Kind::Regular, kGridL1Y0},
    RankRule{"Leopard", kAny, Kind::Regular, kGridL1Y1},
    RankRule{"Jackal", kAny, Kind::Regular, kGridL1Y2},
    RankRule{"Tarantula", kAny, Kind::Regular, kGridL1Y3},
    RankRule{"Tarantula", kAny, Kind::Regular, kGridL1Y4},

    RankRule{"Grizzly", kAny, Kind::Regular, kGridL2Y0},
    RankRule{"Jackal", kAny, Kind::Regular, kGridL2Y1},
    RankRule{"Jackal", kAny, Kind::Regular, kGridL2Y2},
    RankRule{"Jackal", kAny, Kind::Regular, kGridL2Y3},
    RankRule{"Gazelle", kAny, Kind::Regular, kGridL2Y4},

    RankRule{"Grizzly", kAny, Kind::Regular, kGridL3Y0},
    RankRule{"Grizzly", kAny, Kind::Regular, kGridL3Y1},
    RankRule{"Jackal", kAny, Kind::Regular, kGridL3Y2},
    RankRule{"Gazelle", kAny, Kind::Regular, kGridL3Y3},
    RankRule{"Gazelle", kAny, Kind::Regular, kGridL3Y4},
}};

static_assert(std::size(kMgs1Rules) == 23);

} // namespace

std::span<const RankRule> mgs1_rules()
{
    return kMgs1Rules;
}

}
