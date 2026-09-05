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

// Name for an evaluator title id (1..24). These ids are the game's own and
// are NOT the localization indices used by kPwTitles - FOXHOUND is id 10 but
// localization index 13.
const char* pw_title_name(int id);

// One insignia record, by the game's own insignia id (1..110). `over` is the
// value the counter must exceed (the test is strict); -1 means the grant is
// not a simple counter compare. `heroism` is the award. Names come from the
// localization: 0x140544390 formats "sig_%03d_alp_ovl_nearest" and looks the
// hash up in element 0xb906b5, whose rows run in reverse id order. Six ids
// have no English label and read "???" in the game too.
struct PwInsignia {
    const char* name;
    int over;
    int heroism;
};

PwInsignia pw_insignia(int id);

// Candidate grade 0..5 for the title the profile currently matches, plus the
// single gate that stops the next grade. Mirrors the native evaluator: see
// "Codename system" in docs/mgspw_research.md.
struct PwGrade {
    int grade = 0;        // 0 = no grade earned yet
    int next = 0;         // 0 when grade is already 5
    const char* blocker = nullptr;  // what stops `next`, null when nothing does
    double have = 0.0;    // current value of the blocking input
    double need = 0.0;    // value it must reach
};

PwGrade pw_grade(const GameStats& s);

std::optional<Match> evaluate_mgspw(const GameStats& s);

std::vector<ReqStatus> elite_requirements_mgspw(const GameStats& s);

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
