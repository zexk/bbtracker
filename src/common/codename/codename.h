#pragma once

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "../stats.h"

namespace bb::codename {

enum class StatId : uint8_t {
    Kills,
    Alerts,
    Continues,
    Saves,
    RationsUsed,
    ShotsFired,
    DamageBars,
    PlayTimeHours,
    PlayTimeMinutes,
    SevereInjuries,
    LifeMedUsed,
    MealsEaten,
    PlantsCaptured,
    Kerotans,
    LeechAttached,
    TsuchinokoAlive,
    SpecialItemUsed,
    RadarOff,
    MissionCode,
    ClearingEscapes,
    SeaLouse,
    DiscoveryRatio,
    CqcChokes,
    Headshots,
    KnifeDefeats,
    CqcHolds,
    ItemsGiven,
    Praises,
    WeaponsAcquired,
    BodySearches,
    HoldUps,
    BoxTimeMinutes,
    MagazinePages,
    SyringeUses,
    WallTimeMinutes,
    SideRolls,
    ForwardRolls,
    CrawlTimeMinutes,
    CrouchTimeMinutes,
    Pickups,
    CombatHighs,
};

enum class Op : uint8_t {
    Le,
    Lt,
    Ge,
    Gt,
    Eq,
};

enum class Kind : uint8_t {
    Elite,
    Worst,
    Special,
    Regular,
};

struct Cond {
    StatId stat;
    Op op;
    double value;
};

using TierMask = uint8_t;

constexpr TierMask kVe = 1u << 0;
constexpr TierMask kE = 1u << 1;
constexpr TierMask kN = 1u << 2;
constexpr TierMask kH = 1u << 3;
constexpr TierMask kX = 1u << 4;
constexpr TierMask kAllTiers = kVe | kE | kN | kH | kX;

struct RankRule {
    const char* name;
    TierMask tiers;
    Kind kind;
    std::span<const Cond> conds;
    bool needs_time = false;
};

struct Match {
    const char* name;
    Kind kind;
};

double stat_value(const GameStats& s, StatId id);

bool cond_met(const GameStats& s, const Cond& c);

bool rule_matches(const GameStats& s, const RankRule& r, bool skip_tier = false);

std::optional<Match> evaluate_mgs3(const GameStats& s);

std::vector<Match> all_matches_mgs3(const GameStats& s);

const RankRule* find_mgs3(const char* name);

struct ReqStatus {
    const char* label;
    bool pass;
    double current;
    double limit;
    uint8_t fmt; // ReqFmt
    uint8_t op;  // Op
};

enum class ReqFmt : uint8_t {
    Count,
    Bars,
    Time,
};

struct ReqRow {
    const char* label;
    StatId stat;
    Op op;
    double limit;
    ReqFmt fmt;
};

inline ReqStatus make_req_status(const GameStats& s, const ReqRow& row, bool time_gated)
{
    bool pass = cond_met(s, Cond{row.stat, row.op, row.limit});
    if (time_gated && row.fmt == ReqFmt::Time && s.play_time_seconds <= 0.0) {
        pass = false;
    }
    return ReqStatus{row.label, pass, stat_value(s, row.stat), row.limit,
                     static_cast<uint8_t>(row.fmt), static_cast<uint8_t>(row.op)};
}

inline std::vector<ReqStatus> requirements_from_rows(const GameStats& s,
                                                     std::span<const ReqRow> rows,
                                                     bool time_gated)
{
    std::vector<ReqStatus> out;
    out.reserve(rows.size());
    for (const ReqRow& row : rows) {
        out.push_back(make_req_status(s, row, time_gated));
    }
    return out;
}

std::vector<ReqStatus> elite_requirements_mgs3(const GameStats& s);

std::optional<Match> evaluate_mgs2(const GameStats& s);

std::vector<ReqStatus> elite_requirements_mgs2(const GameStats& s);

std::optional<Match> evaluate_mgs4(const GameStats& s);

std::vector<Match> all_matches_mgs4(const GameStats& s);

std::vector<ReqStatus> elite_requirements_mgs4(const GameStats& s);

std::optional<Match> evaluate_mgs1(const GameStats& s);

std::vector<ReqStatus> elite_requirements_mgs1(const GameStats& s);

std::optional<Match> evaluate_mg1(const GameStats& s);

std::vector<ReqStatus> elite_requirements_mg1(const GameStats& s);

std::optional<Match> evaluate_mg2(const GameStats& s);

std::vector<ReqStatus> elite_requirements_mg2(const GameStats& s);

} // namespace bb::codename
