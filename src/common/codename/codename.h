#pragma once

#include <array>
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

// The animal-named ranks MGS2 and MGS3 award per difficulty tier, best tier
// first. Both games use the same names and only differ in tier masks, so
// the strings live here once and each game's rules index them. MGS1
// Integral reuses most of these but renames the lowest fast tier, so it
// keeps its own table.
struct AnimalTierNames {
    const char* top;
    const char* high;
    const char* normal;
    const char* low;
};

struct AnimalTiers {
    AnimalTierNames worst;
    AnimalTierNames low_alerts;
    AnimalTierNames fast;
    AnimalTierNames kills;
    AnimalTierNames meals;
    AnimalTierNames time;
    AnimalTierNames saves;
};

inline constexpr AnimalTiers kAnimalTiers{
    {"Ostrich", "Rabbit", "Mouse", "Chicken"},
    {"Night Owl", "Flying Fox", "Bat", "Flying Squirrel"},
    {"Eagle", "Hawk", "Falcon", "Swallow"},
    {"Orca", "Jaws", "Shark", "Piranha"},
    {"Whale", "Mammoth", "Elephant", "Pig"},
    {"Giant Panda", "Sloth", "Capybara", "Koala"},
    {"Hippopotamus", "Zebra", "Deer", "Cat"},
};

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

// Rule tables are ordered best-first, so the first rule the stats satisfy is
// the rank; all_matches reports every one, which the MGS3 and MGS4 panels use
// to list the special ranks and feats earned alongside it.
std::optional<Match> first_match(const GameStats& s, std::span<const RankRule> rules);

std::vector<Match> all_matches(const GameStats& s, std::span<const RankRule> rules);

std::optional<Match> evaluate_mgs3(const GameStats& s);

std::vector<Match> all_matches_mgs3(const GameStats& s);

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
    Percent,
};

struct ReqRow {
    const char* label;
    StatId stat;
    Op op;
    double limit;
    ReqFmt fmt;
};

// Derive match conds from a display ladder so an elite rank's thresholds
// live in exactly one table: the ReqRow array carries labels and formats
// for the requirements panel, and the rules match on these conds.
template <size_t N>
constexpr std::array<Cond, N> conds_from_rows(const std::array<ReqRow, N>& rows)
{
    std::array<Cond, N> out{};
    for (size_t i = 0; i < N; ++i) {
        out[i] = Cond{rows[i].stat, rows[i].op, rows[i].limit};
    }
    return out;
}

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

std::optional<Match> evaluate_babel(const GameStats& s);

std::vector<ReqStatus> elite_requirements_babel(const GameStats& s);

// Insignia id 1..110; `over` is strict, -1 means no mapped counter.
struct PwInsignia {
    const char* name;
    int over;
    int heroism;
};

PwInsignia pw_insignia(int id);

// Career value used by mapped insignias, or -1 when unavailable.
int pw_insignia_progress(int id, const GameStats& s);

// Candidate grade plus first gate blocking next grade.
struct PwGrade {
    int grade = 0;        // 0 = no grade earned yet
    int next = 0;         // 0 when grade is already 5
    const char* blocker = nullptr;  // what stops `next`, null when nothing does
    double have = 0.0;    // current value of the blocking input
    double need = 0.0;    // value it must reach
    // All-weapons titles are the only ones the Heroism floor gates.
    bool all_weapons = false;
};

PwGrade pw_grade(const GameStats& s);

// Native weapon slots and their six title classes; slot 10 has no class.
struct PwAxes {
    int slot[12] = {};
    int by_class[6] = {};
    int total = 0;
    int lethal = 0;
    int nonlethal = 0;
    int classes_used = 0;
    bool native = false;
};

PwAxes pw_axes(const GameStats& s);

// Class name for a `by_class` index, and the gates one grade demands.
const char* pw_class_name(int cls);

struct PwGradeGate {
    int camaraderie;
    int heroism;
    double coop_ratio;
};

// Camaraderie above this line marks a co-op career for title and grade use.
inline constexpr int kPwCoopCamaraderie = 10000;

// FOXHOUND is a non-lethal title: non-lethal takedowns must beat twice
// lethal. Shared by the evaluator and the overlay's takedown spread.
inline bool pw_nonlethal_beats_lethal(int lethal, int nonlethal)
{
    return nonlethal > 2 * lethal;
}

PwGradeGate pw_grade_gate(int grade); // 1..5

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
