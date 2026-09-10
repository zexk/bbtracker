#include "codename.h"

#include <array>
#include <algorithm>
#include <cmath>

#include "rules_mgs1.h"
#include "rules_mgs2.h"
#include "rules_mgs3.h"
#include "rules_mgs4.h"

namespace bb::codename {
namespace {

constexpr double kDamageUnitsPerBar = 48.0;

} // namespace

double stat_value(const GameStats& s, StatId id)
{
    switch (id) {
    case StatId::Kills: return s.kills;
    case StatId::Alerts: return s.alerts;
    case StatId::Continues: return s.continues;
    case StatId::Saves: return s.saves;
    case StatId::RationsUsed: return s.rations_used;
    case StatId::ShotsFired: return s.shots_fired;
    case StatId::DamageBars:
        return s.damage_taken_bars >= 0 ? s.damage_taken_bars
                                       : s.damage_taken_units / kDamageUnitsPerBar;
    case StatId::PlayTimeHours: return s.play_time_seconds / 3600.0;
    case StatId::PlayTimeMinutes: return std::ceil(s.play_time_seconds / 60.0);
    case StatId::SevereInjuries: return s.severe_injuries;
    case StatId::LifeMedUsed: return s.life_med_used;
    case StatId::MealsEaten: return s.meals_eaten;
    case StatId::PlantsCaptured: return s.plants_captured;
    case StatId::Kerotans: return s.kerotans;
    case StatId::LeechAttached: return s.leech_attached ? 1.0 : 0.0;
    case StatId::TsuchinokoAlive: return s.tsuchinoko_alive ? 1.0 : 0.0;
    case StatId::SpecialItemUsed: return s.special_item_used ? 1.0 : 0.0;
    case StatId::RadarOff: return s.radar_off ? 1.0 : 0.0;
    case StatId::MissionCode: return s.mission;
    case StatId::ClearingEscapes: return s.clearing_escapes;
    case StatId::SeaLouse: return s.sea_louse ? 1.0 : 0.0;
    case StatId::CqcChokes: return s.cqc_chokes;
    case StatId::Headshots: return s.headshots;
    case StatId::KnifeDefeats: return s.knife_defeats;
    case StatId::CqcHolds: return s.cqc_holds;
    case StatId::ItemsGiven: return s.items_given;
    case StatId::Praises: return s.praises;
    case StatId::WeaponsAcquired: return s.weapons_acquired;
    case StatId::BodySearches: return s.body_searches;
    case StatId::HoldUps: return s.hold_ups;
    case StatId::BoxTimeMinutes: return s.box_time_seconds / 60.0;
    case StatId::MagazinePages: return s.magazine_pages;
    case StatId::SyringeUses: return s.syringe_uses;
    case StatId::WallTimeMinutes: return s.wall_time_seconds / 60.0;
    case StatId::SideRolls: return s.side_rolls;
    case StatId::ForwardRolls: return s.forward_rolls;
    case StatId::CrawlTimeMinutes: return s.crawl_time_seconds / 60.0;
    case StatId::CrouchTimeMinutes: return s.crouch_time_seconds / 60.0;
    case StatId::Pickups: return s.pickups;
    case StatId::CombatHighs: return s.combat_highs;
    case StatId::DiscoveryRatio: {
        // Below the kill floor the ratio reads perfect; above it each alert
        // weighs ten against kills past the floor.
        constexpr double kKillFloor = 25.0;
        constexpr double kAlertWeight = 10.0;
        if (s.kills < kKillFloor) {
            return 100.0;
        }
        const double denom = static_cast<double>(s.kills) - kKillFloor;
        return static_cast<double>(s.alerts) * kAlertWeight / (denom < 1.0 ? 1.0 : denom);
    }
    }
    return 0.0;
}

bool cond_met(const GameStats& s, const Cond& c)
{
    const double v = stat_value(s, c.stat);
    switch (c.op) {
    case Op::Le: return v <= c.value;
    case Op::Lt: return v < c.value;
    case Op::Ge: return v >= c.value;
    case Op::Gt: return v > c.value;
    case Op::Eq: return v == c.value;
    }
    return false;
}

TierMask tier_bit(Difficulty d)
{
    if (d == Difficulty::VeryEasy) return kVe;
    if (d == Difficulty::Easy) return kE;
    if (d == Difficulty::Normal) return kN;
    if (d == Difficulty::Hard) return kH;
    if (d == Difficulty::EuroExtreme) return 1u << 5;
    return kX;
}

bool rule_matches(const GameStats& s, const RankRule& r, bool skip_tier)
{
    if (!skip_tier && (r.tiers & tier_bit(s.difficulty)) == 0) {
        return false;
    }
    if (r.needs_time && s.play_time_seconds <= 0.0) {
        return false;
    }
    for (const Cond& c : r.conds) {
        if (!cond_met(s, c)) {
            return false;
        }
    }
    return true;
}

std::optional<Match> first_match(const GameStats& s, std::span<const RankRule> rules)
{
    for (const RankRule& r : rules) {
        if (rule_matches(s, r)) {
            return Match{r.name, r.kind};
        }
    }
    return std::nullopt;
}

std::vector<Match> all_matches(const GameStats& s, std::span<const RankRule> rules)
{
    std::vector<Match> out;
    for (const RankRule& r : rules) {
        if (rule_matches(s, r)) {
            out.push_back(Match{r.name, r.kind});
        }
    }
    return out;
}

std::optional<Match> evaluate_mgs3(const GameStats& s)
{
    return first_match(s, mgs3_rules());
}

std::vector<Match> all_matches_mgs3(const GameStats& s)
{
    return all_matches(s, mgs3_rules());
}

const RankRule* find_mgs3(const char* name)
{
    for (const RankRule& r : mgs3_rules()) {
        if (std::string_view(r.name) == name) {
            return &r;
        }
    }
    return nullptr;
}

std::vector<ReqStatus> elite_requirements_mgs3(const GameStats& s)
{
    return requirements_from_rows(s, mgs3_elite_rows(), false);
}

std::optional<Match> evaluate_mgs2(const GameStats& s)
{
    return first_match(s, mgs2_rules());
}

std::vector<ReqStatus> elite_requirements_mgs2(const GameStats& s)
{
    return requirements_from_rows(s, mgs2_elite_rows(), false);
}

std::optional<Match> evaluate_mgs1(const GameStats& s)
{
    const std::span<const RankRule> rules = s.mgs1_integral ? mgs1_integral_rules() : mgs1_rules();
    for (const RankRule& r : rules) {
        // Japanese original has no difficulty choice, so its elite ranks carry
        // no tier gate; US/EU gate FOX to Hard and BIG BOSS to Extreme.
        const bool jp_ungated = !s.mgs1_integral && s.mgs1_japanese_original
            && r.kind == Kind::Elite;
        if (rule_matches(s, r, jp_ungated)) {
            return Match{r.name, r.kind};
        }
    }
    return std::nullopt;
}

std::vector<ReqStatus> elite_requirements_mgs1(const GameStats& s)
{
    const std::span<const ReqRow> ladder = mgs1_elite_rows();
    return requirements_from_rows(s, s.mgs1_integral ? ladder.subspan(1) : ladder, true);
}

namespace {

constexpr std::array<ReqRow, 6> kMg1Reqs{{
    {"play time", StatId::PlayTimeHours, Op::Lt, 50.0 / 60.0, ReqFmt::Time},
    {"continues", StatId::Continues, Op::Eq, 0, ReqFmt::Count},
    {"alerts", StatId::Alerts, Op::Le, 8, ReqFmt::Count},
    {"kills", StatId::Kills, Op::Eq, 0, ReqFmt::Count},
    {"rations used", StatId::RationsUsed, Op::Le, 1, ReqFmt::Count},
    {"special items", StatId::SpecialItemUsed, Op::Eq, 0, ReqFmt::Count},
}};

constexpr std::array<ReqRow, 6> kMg2Reqs{{
    {"play time", StatId::PlayTimeHours, Op::Lt, 1.75, ReqFmt::Time},
    {"continues", StatId::Continues, Op::Eq, 0, ReqFmt::Count},
    {"alerts", StatId::Alerts, Op::Le, 6, ReqFmt::Count},
    {"kills", StatId::Kills, Op::Le, 5, ReqFmt::Count},
    {"rations used", StatId::RationsUsed, Op::Eq, 0, ReqFmt::Count},
    {"special items", StatId::SpecialItemUsed, Op::Eq, 0, ReqFmt::Count},
}};

// MG1 and MG2 have a single elite rank each, earned by clearing every row of
// the ladder. Extreme names it BIG BOSS, Easy names it FOX; no other
// difficulty ranks at all.
std::optional<Match> classic_elite(const GameStats& s, std::span<const ReqRow> rows)
{
    if (s.difficulty != Difficulty::Extreme && s.difficulty != Difficulty::Easy) {
        return std::nullopt;
    }
    const std::vector<ReqStatus> reqs = requirements_from_rows(s, rows, true);
    if (!std::ranges::all_of(reqs, [](const ReqStatus& r) { return r.pass; })) {
        return std::nullopt;
    }
    return Match{s.difficulty == Difficulty::Extreme ? "BIG BOSS" : "FOX", Kind::Elite};
}

} // namespace

std::optional<Match> evaluate_mg1(const GameStats& s)
{
    if (const auto elite = classic_elite(s, kMg1Reqs)) return elite;

    const double seconds = s.play_time_seconds;
    if (seconds < 50 * 60 && s.kills <= 3) return Match{"EAGLE", Kind::Regular};
    if (seconds < 90 * 60 && s.kills <= 3) return Match{"PANTHER", Kind::Regular};
    if (seconds < 120 * 60 && s.kills <= 3) return Match{"JACKAL", Kind::Regular};
    if (seconds < 240 * 60 && s.kills <= 3) return Match{"ZEBRA", Kind::Regular};
    if (seconds < 480 * 60) return Match{"DEER", Kind::Regular};
    if (seconds < 720 * 60) return Match{"ELEPHANT", Kind::Regular};
    if (seconds < 960 * 60) return Match{"HIPPOPOTAMUS", Kind::Regular};
    if (seconds < 1200 * 60) return Match{"TURTLE", Kind::Regular};
    return Match{"CHICKEN", Kind::Worst};
}

std::vector<ReqStatus> elite_requirements_mg1(const GameStats& s)
{
    return requirements_from_rows(s, kMg1Reqs, true);
}

std::optional<Match> evaluate_mg2(const GameStats& s)
{
    if (const auto elite = classic_elite(s, kMg2Reqs)) return elite;

    const double seconds = s.play_time_seconds;
    if (seconds < 105 * 60 && s.kills <= 10) return Match{"EAGLE", Kind::Regular};
    if (seconds < 150 * 60 && s.kills <= 10) return Match{"PANTHER", Kind::Regular};
    if (seconds < 240 * 60 && s.kills <= 10) return Match{"JACKAL", Kind::Regular};
    if (seconds < 480 * 60) return Match{"ZEBRA", Kind::Regular};
    if (seconds < 720 * 60) return Match{"DEER", Kind::Regular};
    if (seconds < 900 * 60) return Match{"ELEPHANT", Kind::Regular};
    if (seconds < 1080 * 60) return Match{"HIPPOPOTAMUS", Kind::Regular};
    if (seconds < 1440 * 60) return Match{"TURTLE", Kind::Regular};
    return Match{"CHICKEN", Kind::Worst};
}

std::vector<ReqStatus> elite_requirements_mg2(const GameStats& s)
{
    return requirements_from_rows(s, kMg2Reqs, true);
}

namespace {

constexpr std::array<ReqRow, 4> kBabelReqs{{
    {"play time", StatId::PlayTimeHours, Op::Le, 2.0, ReqFmt::Time},
    {"alerts", StatId::Alerts, Op::Lt, 6, ReqFmt::Count},
    {"kills", StatId::Kills, Op::Lt, 25, ReqFmt::Count},
    {"rations used", StatId::RationsUsed, Op::Lt, 2, ReqFmt::Count},
}};

constexpr int category(int value, std::array<int, 4> thresholds)
{
    return static_cast<int>(std::ranges::count_if(
        thresholds, [value](int threshold) { return value >= threshold; }));
}

} // namespace

std::optional<Match> evaluate_babel(const GameStats& s)
{
    if (s.difficulty_raw > 3 || s.play_time_seconds <= 0.0) {
        return std::nullopt;
    }

    const int found = category(s.alerts, {6, 26, 61, 121});
    const int kills = category(s.kills, {25, 60, 120, 200});
    const int ration_limit[] = {34, 33, 21, 12};
    const int rations = category(s.rations_used, {2, 2, 2, ration_limit[s.difficulty_raw]});
    const int saves = category(s.saves, {80, 80, 80, 80});
    const int seconds = static_cast<int>(s.play_time_seconds);
    const int time = category(seconds, {5401, 7201, 36000, 36000});

    int rank;
    if (found == 0 && kills == 0 && rations == 0 && time < 2) rank = 0;
    else if (kills == 4 && rations == 4 && saves == 4 && time == 4) rank = 1;
    else if (time == 0) rank = 2;
    else if (found == 4) rank = 3;
    else if (kills == 4) rank = 4;
    else if (rations == 4) rank = 5;
    else if (time == 4) rank = 6;
    else {
        constexpr int table[3][3]{{7, 8, 9}, {10, 8, 8}, {10, 8, 11}};
        rank = table[found ? found - 1 : 0][kills ? kills - 1 : 0];
    }

    constexpr const char* names[4][12]{
        {"HOUND", "CHICKEN", "SPARROW", "CICADA", "PIRANHA", "PIG", "SNAIL",
         "SPIDER", "MONGOOSE", "PUMA", "BEAVER", "CHAMELEON"},
        {"DOBERMAN", "MOUSE", "PIGEON", "MYNA", "SHARK", "ELEPHANT", "TURTLE",
         "TARANTURA", "HYENA", "LEOPARD", "BAT", "IGUANA"},
        {"FOX", "RABBIT", "SWALLOW", "PARROT", "JAWS", "MAMMOTH", "KOALA",
         "CENTIPIDE", "JACKAL", "PANTHER", "MOLE", "ALLIGATOR"},
        {"BIG BOSS", "OSTRICH", "FALCON", "PEACOCK", "ORCA", "WHALE", "SLOTH",
         "SCORPION", "COYOTE", "JAGUAR", "CLOW", "CROCODILE"},
    };
    return Match{names[s.difficulty_raw][rank],
                 rank == 0 ? Kind::Elite : rank == 1 ? Kind::Worst : Kind::Regular};
}

std::vector<ReqStatus> elite_requirements_babel(const GameStats& s)
{
    return requirements_from_rows(s, kBabelReqs, true);
}

} // namespace bb::codename
