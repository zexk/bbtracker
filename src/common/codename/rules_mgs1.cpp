#include "codename.h"

#include <array>

namespace bb::codename {
namespace {

constexpr TierMask kAny = kVe | kE | kN | kH | kX;

// Single source for the elite ladder: the requirements panel reads the
// rows, the rank rules match on the derived conds. Integral matches on
// everything past the radar row.
constexpr std::array<ReqRow, 6> kEliteLadder{{
    {"radar", StatId::RadarOff, Op::Eq, 1, ReqFmt::Count},
    {"discovered", StatId::Alerts, Op::Lt, 4, ReqFmt::Count},
    {"kills", StatId::Kills, Op::Lt, 25, ReqFmt::Count},
    {"rations used", StatId::RationsUsed, Op::Le, 1, ReqFmt::Count},
    {"continues", StatId::Continues, Op::Eq, 0, ReqFmt::Count},
    {"play time", StatId::PlayTimeHours, Op::Lt, 3, ReqFmt::Time},
}};

constexpr std::array<Cond, 6> kEliteConds = conds_from_rows(kEliteLadder);

constexpr Cond kFalcon[] = {{StatId::PlayTimeHours, Op::Lt, 2.5}};
constexpr Cond kJaws[] = {{StatId::Kills, Op::Gt, 250}};
constexpr Cond kPig[] = {{StatId::RationsUsed, Op::Gt, 120}};
constexpr Cond kHippo[] = {{StatId::Saves, Op::Gt, 80}};
constexpr Cond kTurtle[] = {{StatId::PlayTimeHours, Op::Gt, 18}};
constexpr Cond kChicken[] = {{StatId::RationsUsed, Op::Gt, 120},
                             {StatId::Saves, Op::Gt, 80},
                             {StatId::PlayTimeHours, Op::Gt, 18}};

const std::array<RankRule, 8> kMgs1Rules{{
    RankRule{"BIG BOSS", kX, Kind::Elite, std::span(kEliteConds)},
    RankRule{"FOX", kH, Kind::Elite, std::span(kEliteConds).subspan(1), true},

    RankRule{"Falcon", kAny, Kind::Special, kFalcon, true},
    RankRule{"Jaws", kAny, Kind::Special, kJaws},
    RankRule{"Pig", kAny, Kind::Special, kPig},
    RankRule{"Hippopotamus", kAny, Kind::Special, kHippo},
    RankRule{"Turtle", kAny, Kind::Special, kTurtle, true},
    RankRule{"Chicken", kAny, Kind::Special, kChicken, true},
}};

} // namespace

std::span<const RankRule> mgs1_rules()
{
    return kMgs1Rules;
}

std::span<const ReqRow> mgs1_elite_rows()
{
    return kEliteLadder;
}

std::span<const Cond> mgs1_elite_conds()
{
    return kEliteConds;
}

const char* mgs1_regular_name(const GameStats& s)
{
    if (s.alerts < 1) return nullptr;
    constexpr const char* names[3][5] = {
        {"Leopard", "Leopard", "Jackal", "Tarantula", "Tarantula"},
        {"Grizzly", "Jackal", "Jackal", "Jackal", "Gazelle"},
        {"Grizzly", "Grizzly", "Jackal", "Gazelle", "Gazelle"},
    };
    const int x = s.alerts < 30 ? 0 : s.alerts < 55 ? 1 : 2;
    const double ratio = stat_value(s, StatId::DiscoveryRatio);
    const int y = ratio < 4 ? 0 : ratio < 8 ? 1 : ratio < 16 ? 2 : ratio < 20 ? 3 : 4;
    return names[x][y];
}

}
