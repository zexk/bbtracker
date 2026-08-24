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

bool rule_matches(const GameStats& s, const RankRule& r)
{
    if ((r.tiers & tier_bit(s.difficulty)) == 0) {
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

struct ReqRow {
    const char* label;
    StatId stat;
    Op op;
    double limit;
    ReqFmt fmt;
};

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
    std::vector<ReqStatus> out;
    for (const ReqRow& row : kFoxhoundReqs) {
        out.push_back(ReqStatus{
            row.label,
            cond_met(s, Cond{row.stat, row.op, row.limit}),
            stat_value(s, row.stat),
            row.limit,
            static_cast<uint8_t>(row.fmt),
            static_cast<uint8_t>(row.op),
        });
    }
    return out;
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

struct Mgs2ReqRow {
    const char* label;
    StatId stat;
    Op op;
    double limit;
    ReqFmt fmt;
};

constexpr std::array<Mgs2ReqRow, 10> kBigBossReqs{{
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
    std::vector<ReqStatus> out;
    for (const Mgs2ReqRow& row : kBigBossReqs) {
        out.push_back(ReqStatus{
            row.label,
            cond_met(s, Cond{row.stat, row.op, row.limit}),
            stat_value(s, row.stat),
            row.limit,
            static_cast<uint8_t>(row.fmt),
            static_cast<uint8_t>(row.op),
        });
    }
    return out;
}

std::optional<Match> evaluate_mgs1(const GameStats& s)
{
    const std::span<const RankRule> rules = s.mgs1_integral ? mgs1_integral_rules() : mgs1_rules();
    for (const RankRule& r : rules) {
        bool matches = rule_matches(s, r);
        if (!matches && !s.mgs1_integral && s.mgs1_japanese_original && r.kind == Kind::Elite) {
            GameStats jp = s;
            jp.difficulty = r.name[0] == 'B' ? Difficulty::Extreme : Difficulty::Hard;
            matches = rule_matches(jp, r);
        }
        if (matches) {
            return Match{r.name, r.kind};
        }
    }
    return std::nullopt;
}

namespace {

struct Mgs1ReqRow {
    const char* label;
    StatId stat;
    Op op;
    double limit;
    ReqFmt fmt;
};

constexpr std::array<Mgs1ReqRow, 6> kOriginalLadderReqs{{
    {"radar", StatId::RadarOff, Op::Eq, 1, ReqFmt::Count},
    {"discovered", StatId::Alerts, Op::Lt, 4, ReqFmt::Count},
    {"kills", StatId::Kills, Op::Lt, 25, ReqFmt::Count},
    {"rations used", StatId::RationsUsed, Op::Le, 1, ReqFmt::Count},
    {"continues", StatId::Continues, Op::Eq, 0, ReqFmt::Count},
    {"play time", StatId::PlayTimeHours, Op::Lt, 3, ReqFmt::Time},
}};

constexpr std::array<Mgs1ReqRow, 5> kIntegralLadderReqs{{
    {"discovered", StatId::Alerts, Op::Lt, 4, ReqFmt::Count},
    {"kills", StatId::Kills, Op::Lt, 25, ReqFmt::Count},
    {"rations used", StatId::RationsUsed, Op::Le, 1, ReqFmt::Count},
    {"continues", StatId::Continues, Op::Eq, 0, ReqFmt::Count},
    {"play time", StatId::PlayTimeHours, Op::Lt, 3, ReqFmt::Time},
}};

} // namespace

std::vector<ReqStatus> elite_requirements_mgs1(const GameStats& s)
{
    std::vector<ReqStatus> out;
    const std::span<const Mgs1ReqRow> rows =
        s.mgs1_integral ? std::span<const Mgs1ReqRow>{kIntegralLadderReqs}
                        : std::span<const Mgs1ReqRow>{kOriginalLadderReqs};
    for (const Mgs1ReqRow& row : rows) {
        bool pass = cond_met(s, Cond{row.stat, row.op, row.limit});
        if (row.fmt == ReqFmt::Time && s.play_time_seconds <= 0.0) {
            pass = false;
        }
        out.push_back(ReqStatus{
            row.label,
            pass,
            stat_value(s, row.stat),
            row.limit,
            static_cast<uint8_t>(row.fmt),
            static_cast<uint8_t>(row.op),
        });
    }
    return out;
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

std::vector<ReqStatus> mg_requirements(const GameStats& s, std::span<const ReqRow> rows)
{
    std::vector<ReqStatus> out;
    for (const ReqRow& row : rows) {
        bool pass = cond_met(s, Cond{row.stat, row.op, row.limit});
        if (row.fmt == ReqFmt::Time && s.play_time_seconds <= 0.0) {
            pass = false;
        }
        out.push_back({row.label, pass, stat_value(s, row.stat), row.limit,
                       static_cast<uint8_t>(row.fmt), static_cast<uint8_t>(row.op)});
    }
    return out;
}

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
    return mg_requirements(s, kMg1Reqs);
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
    return mg_requirements(s, kMg2Reqs);
}

} // namespace bb::codename
