#include "codename.h"

#include <array>
#include <cmath>

#include "rules_mgs1.h"
#include "rules_mgs2.h"
#include "rules_mgs3.h"

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
        if (s.kills < 25) {
            return 100.0;
        }
        const double denom = static_cast<double>(s.kills - 25);
        return static_cast<double>(s.alerts) * 10.0 / (denom < 1.0 ? 1.0 : denom);
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

namespace {

constexpr std::array<RankRule, 0> kNone{};

} // namespace

std::optional<Match> evaluate_mgs3(const GameStats& s)
{
    for (const RankRule& r : mgs3_rules()) {
        if (rule_matches(s, r)) {
            return Match{r.name, r.kind};
        }
    }
    return std::nullopt;
}

std::vector<Match> all_matches_mgs3(const GameStats& s)
{
    std::vector<Match> out;
    for (const RankRule& r : mgs3_rules()) {
        if (rule_matches(s, r)) {
            out.push_back(Match{r.name, r.kind});
        }
    }
    return out;
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

namespace {

constexpr std::array<ReqRow, 9> kFoxhoundReqs{{
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

} // namespace

std::vector<ReqStatus> elite_requirements_mgs3(const GameStats& s)
{
    return requirements_from_rows(s, kFoxhoundReqs, false);
}

std::optional<Match> evaluate_mgs2(const GameStats& s)
{
    for (const RankRule& r : mgs2_rules()) {
        if (rule_matches(s, r)) {
            return Match{r.name, r.kind};
        }
    }
    return std::nullopt;
}

namespace {

constexpr std::array<ReqRow, 10> kBigBossReqs{{
    {"special items", StatId::SpecialItemUsed, Op::Eq, 0, ReqFmt::Count},
    {"radar", StatId::RadarOff, Op::Eq, 1, ReqFmt::Count},
    {"shots fired", StatId::ShotsFired, Op::Le, 700, ReqFmt::Count},
    {"alerts", StatId::Alerts, Op::Le, 3, ReqFmt::Count},
    {"damage", StatId::DamageBars, Op::Le, 10, ReqFmt::Bars},
    {"kills", StatId::Kills, Op::Eq, 0, ReqFmt::Count},
    {"rations used", StatId::RationsUsed, Op::Eq, 0, ReqFmt::Count},
    {"play time", StatId::PlayTimeHours, Op::Le, 3, ReqFmt::Time},
    {"continues", StatId::Continues, Op::Eq, 0, ReqFmt::Count},
    {"saves", StatId::Saves, Op::Le, 8, ReqFmt::Count},
}};

} // namespace

std::vector<ReqStatus> elite_requirements_mgs2(const GameStats& s)
{
    return requirements_from_rows(s, kBigBossReqs, false);
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

namespace {

constexpr std::array<ReqRow, 6> kOriginalLadderReqs{{
    {"radar", StatId::RadarOff, Op::Eq, 1, ReqFmt::Count},
    {"discovered", StatId::Alerts, Op::Lt, 4, ReqFmt::Count},
    {"kills", StatId::Kills, Op::Lt, 25, ReqFmt::Count},
    {"rations used", StatId::RationsUsed, Op::Le, 1, ReqFmt::Count},
    {"continues", StatId::Continues, Op::Eq, 0, ReqFmt::Count},
    {"play time", StatId::PlayTimeHours, Op::Lt, 3, ReqFmt::Time},
}};

constexpr std::array<ReqRow, 5> kIntegralLadderReqs{{
    {"discovered", StatId::Alerts, Op::Lt, 4, ReqFmt::Count},
    {"kills", StatId::Kills, Op::Lt, 25, ReqFmt::Count},
    {"rations used", StatId::RationsUsed, Op::Le, 1, ReqFmt::Count},
    {"continues", StatId::Continues, Op::Eq, 0, ReqFmt::Count},
    {"play time", StatId::PlayTimeHours, Op::Lt, 3, ReqFmt::Time},
}};

} // namespace

std::vector<ReqStatus> elite_requirements_mgs1(const GameStats& s)
{
    const std::span<const ReqRow> rows =
        s.mgs1_integral ? std::span<const ReqRow>{kIntegralLadderReqs}
                        : std::span<const ReqRow>{kOriginalLadderReqs};
    return requirements_from_rows(s, rows, true);
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

bool all_pass(const std::vector<ReqStatus>& requirements)
{
    for (const ReqStatus& requirement : requirements) {
        if (!requirement.pass) return false;
    }
    return true;
}

} // namespace

std::optional<Match> evaluate_mg1(const GameStats& s)
{
    if ((s.difficulty == Difficulty::Extreme || s.difficulty == Difficulty::Easy)
        && all_pass(elite_requirements_mg1(s))) {
        return Match{s.difficulty == Difficulty::Extreme ? "BIG BOSS" : "FOX", Kind::Elite};
    }
    return std::nullopt;
}

std::vector<ReqStatus> elite_requirements_mg1(const GameStats& s)
{
    return requirements_from_rows(s, kMg1Reqs, true);
}

std::optional<Match> evaluate_mg2(const GameStats& s)
{
    if ((s.difficulty == Difficulty::Extreme || s.difficulty == Difficulty::Easy)
        && all_pass(elite_requirements_mg2(s))) {
        return Match{s.difficulty == Difficulty::Extreme ? "BIG BOSS" : "FOX", Kind::Elite};
    }
    return std::nullopt;
}

std::vector<ReqStatus> elite_requirements_mg2(const GameStats& s)
{
    return requirements_from_rows(s, kMg2Reqs, true);
}

// --- Peace Walker -----------------------------------------------------------
//
// PW does not score a codename from thresholds the way MGS1-4 do. It
// classifies career play on three axes and hands out the matching title from a
// 24-entry table; the table below is the game's own, recovered from
// localization block slot/001FC/3 (names at 0-23, descriptions at 24+index).
//
// The axes are the game's, the cut-offs are ours: PW's own classifier has not
// been located, so "all weapons" and the lethal/non-lethal balance are read
// from the career counters with the thresholds documented here.

namespace {

enum class WeaponClass : uint8_t { Short, Medium, Long, Explosive, Cqc, Stun, All };

struct PwTitle {
    const char* name;
    WeaponClass cls;
    bool coop;
    bool nonlethal;
};

// Index is the game's own title index; FOX (14) is the solo counterpart to
// FOXHOUND and the only slot whose name string the extraction did not carry.
constexpr std::array<PwTitle, 24> kPwTitles{{
    {"WOLF",      WeaponClass::Medium,    true,  false},
    {"WHALE",     WeaponClass::Explosive, true,  true},
    {"SWALLOW",   WeaponClass::Long,      false, true},
    {"SCORPION",  WeaponClass::Short,     false, false},
    {"RAVEN",     WeaponClass::Long,      true,  false},
    {"PUMA",      WeaponClass::Medium,    false, false},
    {"PIRANHA",   WeaponClass::Explosive, true,  false},
    {"ORCA",      WeaponClass::Explosive, false, false},
    {"OCTOPUS",   WeaponClass::Explosive, false, true},
    {"KANGAROO",  WeaponClass::Cqc,       true,  true},
    {"HOUND",     WeaponClass::All,       false, false},
    {"HAWK",      WeaponClass::Long,      false, false},
    {"GULL",      WeaponClass::Long,      true,  true},
    {"FOXHOUND",  WeaponClass::All,       true,  true},
    {"FOX",       WeaponClass::All,       false, true},
    {"FIREFLY",   WeaponClass::Stun,      true,  true},
    {"EEL",       WeaponClass::Stun,      false, true},
    {"DOBERMAN",  WeaponClass::All,       true,  false},
    {"DEER",      WeaponClass::Medium,    true,  true},
    {"CAT",       WeaponClass::Medium,    false, true},
    {"BUTTERFLY", WeaponClass::Short,     false, true},
    {"BEE",       WeaponClass::Short,     true,  false},
    {"BEAR",      WeaponClass::Cqc,       false, true},
    {"ANT",       WeaponClass::Short,     true,  true},
}};

// A counter reads -1 until its descriptor id resolves; treat that as zero so a
// partially resolved profile still classifies instead of vanishing.
constexpr int pw_count(int value)
{
    return value > 0 ? value : 0;
}

struct PwProfile {
    int by_class[6] = {};
    int total = 0;
    int top = 0;
    int classes_used = 0;
    WeaponClass dominant = WeaponClass::All;
    int nonlethal = 0;
    int lethal = 0;
};

// "All weapons" is the elite branch: no class may hold this share or more of
// career takedowns, and this many classes must be in use.
constexpr double kPwSpreadShare = 0.40;
constexpr int kPwSpreadClasses = 4;

PwProfile pw_profile(const GameStats& s)
{
    PwProfile p;
    p.by_class[static_cast<int>(WeaponClass::Short)] =
        pw_count(s.pw_pistol_takedowns) + pw_count(s.pw_pistol_lethal)
        + pw_count(s.pw_shotgun_takedowns);
    p.by_class[static_cast<int>(WeaponClass::Medium)] =
        pw_count(s.pw_ar_takedowns) + pw_count(s.pw_lmg_takedowns);
    p.by_class[static_cast<int>(WeaponClass::Long)] =
        pw_count(s.pw_sniper_takedowns) + pw_count(s.pw_sniper_nonlethal);
    p.by_class[static_cast<int>(WeaponClass::Explosive)] =
        pw_count(s.pw_grenade_takedowns) + pw_count(s.pw_rocket_takedowns)
        + pw_count(s.pw_placed_takedowns);
    p.by_class[static_cast<int>(WeaponClass::Cqc)] = pw_count(s.pw_cqc_takedowns);
    p.by_class[static_cast<int>(WeaponClass::Stun)] = 0; // no stun-rod id yet

    int top_index = 0;
    for (int i = 0; i < 6; ++i) {
        p.total += p.by_class[i];
        if (p.by_class[i] > 0) ++p.classes_used;
        if (p.by_class[i] > p.top) {
            p.top = p.by_class[i];
            top_index = i;
        }
    }
    const bool spread = p.total > 0
        && p.classes_used >= kPwSpreadClasses
        && static_cast<double>(p.top) < static_cast<double>(p.total) * kPwSpreadShare;
    p.dominant = spread ? WeaponClass::All : static_cast<WeaponClass>(top_index);
    p.nonlethal = pw_count(s.pw_tranq) + pw_count(s.pw_cqc_takedowns);
    p.lethal = pw_count(s.pw_kills);
    return p;
}

} // namespace

std::optional<Match> evaluate_mgspw(const GameStats& s)
{
    const PwProfile p = pw_profile(s);
    if (p.total <= 0) {
        return std::nullopt;
    }
    // No resolved counter distinguishes co-op from solo play, and co-op runs
    // share the same save, so the axis cannot be read either way. Report the
    // solo name; the co-op counterpart (FOXHOUND for FOX) sits at the same
    // weapon-class and force coordinates and stays reachable.
    const bool nonlethal = p.nonlethal > p.lethal;
    for (const PwTitle& t : kPwTitles) {
        if (t.cls == p.dominant && !t.coop && t.nonlethal == nonlethal) {
            return Match{t.name, t.cls == WeaponClass::All ? Kind::Elite : Kind::Regular};
        }
    }
    return std::nullopt;
}

std::vector<ReqStatus> elite_requirements_mgspw(const GameStats& s)
{
    const PwProfile p = pw_profile(s);
    const auto row = [](const char* label, bool pass, double current, double limit) {
        return ReqStatus{label, pass, current, limit,
                         static_cast<uint8_t>(ReqFmt::Count),
                         static_cast<uint8_t>(Op::Ge)};
    };
    std::vector<ReqStatus> out;
    // FOX is the solo all-weapons non-lethal title: spread the takedowns, keep
    // no class dominant, and stay non-lethal.
    out.push_back(row("weapon classes used", p.classes_used >= kPwSpreadClasses,
                      p.classes_used, kPwSpreadClasses));
    const double top_share = p.total > 0 ? 100.0 * p.top / p.total : 0.0;
    out.push_back(ReqStatus{"top class share %", top_share < kPwSpreadShare * 100.0,
                            top_share, kPwSpreadShare * 100.0,
                            static_cast<uint8_t>(ReqFmt::Count),
                            static_cast<uint8_t>(Op::Le)});
    // Shown against kills, the value it actually has to beat.
    out.push_back(row("non-lethal vs kills", p.nonlethal > p.lethal, p.nonlethal,
                      p.lethal));
    return out;
}

} // namespace bb::codename
