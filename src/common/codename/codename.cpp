#include "codename.h"

#include <array>

#include "rules_mgs3.h"

namespace bb::codename {

double stat_value(const GameStats& s, StatId id)
{
    switch (id) {
    case StatId::Kills: return s.kills;
    case StatId::Alerts: return s.alerts;
    case StatId::Continues: return s.continues;
    case StatId::Saves: return s.saves;
    case StatId::RationsUsed: return s.rations_used;
    case StatId::ShotsFired: return s.shots_fired;
    case StatId::DamageBars: return s.damage_taken_bars;
    case StatId::PlayTimeHours: return s.play_time_seconds / 3600.0;
    case StatId::SevereInjuries: return s.severe_injuries;
    case StatId::LifeMedUsed: return s.life_med_used;
    case StatId::MealsEaten: return s.meals_eaten;
    case StatId::SpecialItemUsed: return s.special_item_used ? 1.0 : 0.0;
    case StatId::RadarOff: return s.radar_off ? 1.0 : 0.0;
    case StatId::KerotanAllShot: return s.kerotan_all_shot ? 1.0 : 0.0;
    case StatId::MarkhorComplete: return s.markhor_complete ? 1.0 : 0.0;
    case StatId::TsuchinokoCarried: return s.tsuchinoko_carried ? 1.0 : 0.0;
    case StatId::LeechCarried: return s.leech_carried ? 1.0 : 0.0;
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
    return kX;
}

bool rule_matches(const GameStats& s, const RankRule& r)
{
    if ((r.tiers & tier_bit(s.difficulty)) == 0) {
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
};

constexpr std::array<ReqRow, 8> kFoxhoundReqs{{
    {"alerts", StatId::Alerts, Op::Eq, 0},
    {"kills", StatId::Kills, Op::Eq, 0},
    {"severe injuries", StatId::SevereInjuries, Op::Lt, 20},
    {"damage (bars)", StatId::DamageBars, Op::Lt, 5},
    {"life medicine", StatId::LifeMedUsed, Op::Eq, 0},
    {"play time", StatId::PlayTimeHours, Op::Lt, 5},
    {"continues", StatId::Continues, Op::Eq, 0},
    {"saves", StatId::Saves, Op::Lt, 25},
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
        });
    }
    return out;
}

} // namespace bb::codename
