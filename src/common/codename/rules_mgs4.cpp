#include "rules_mgs4.h"

#include <array>
#include <utility>
#include <vector>

namespace bb::codename {
namespace {

constexpr TierMask kHardUp = kH | kX;
constexpr TierMask kSolidUp = kN | kHardUp;
constexpr TierMask kNakedUp = kE | kSolidUp;

std::vector<RankRule> build_rules(std::vector<std::vector<Cond>>& pool)
{
    std::vector<RankRule> rules;
    auto add = [&](const char* name, TierMask tiers, Kind kind,
                   std::vector<Cond> conds, bool needs_time = false) {
        pool.push_back(std::move(conds));
        rules.push_back({name, tiers, kind, pool.back(), needs_time});
    };

    const auto elite = [&](const char* name, TierMask tiers, int alerts,
                           double hours, bool strict_alerts = false) {
        add(name, tiers, Kind::Elite,
            {{StatId::Alerts, strict_alerts ? Op::Eq : Op::Le, static_cast<double>(alerts)},
             {StatId::Kills, Op::Eq, 0},
             {StatId::Continues, Op::Eq, 0},
             {StatId::RationsUsed, Op::Eq, 0},
             {StatId::PlayTimeHours, Op::Le, hours},
             {StatId::SpecialItemUsed, Op::Eq, 0}}, true);
    };
    elite("BIG BOSS", kX, 0, 5, true);
    elite("FOX HOUND", kHardUp, 3, 5.5);
    elite("FOX", kSolidUp, 5, 6);
    elite("HOUND", kNakedUp, 10, 6.5);

    add("MANTIS", kAllTiers, Kind::Elite,
        {{StatId::Alerts, Op::Eq, 0}, {StatId::Continues, Op::Eq, 0},
         {StatId::RationsUsed, Op::Eq, 0}, {StatId::PlayTimeHours, Op::Le, 5}}, true);
    add("WOLF", kAllTiers, Kind::Elite,
        {{StatId::Continues, Op::Eq, 0}, {StatId::RationsUsed, Op::Eq, 0}});
    add("RAVEN", kAllTiers, Kind::Elite, {{StatId::PlayTimeHours, Op::Le, 5}}, true);
    add("OCTOPUS", kAllTiers, Kind::Elite, {{StatId::Alerts, Op::Eq, 0}});

    add("BEAR", kAllTiers, Kind::Special, {{StatId::CqcChokes, Op::Ge, 100}});
    add("EAGLE", kAllTiers, Kind::Special, {{StatId::Headshots, Op::Ge, 150}});
    add("ASSASSIN", kAllTiers, Kind::Special,
        {{StatId::KnifeDefeats, Op::Ge, 50}, {StatId::CqcHolds, Op::Ge, 50},
         {StatId::Alerts, Op::Le, 25}});
    add("PIGEON", kAllTiers, Kind::Special, {{StatId::Kills, Op::Eq, 0}});
    add("BLUE BIRD", kAllTiers, Kind::Special, {{StatId::ItemsGiven, Op::Ge, 50}});
    add("HAWK", kAllTiers, Kind::Special, {{StatId::Praises, Op::Ge, 25}});
    add("LITTLE GRAY", kAllTiers, Kind::Special, {{StatId::WeaponsAcquired, Op::Ge, 69}});
    add("ANT", kAllTiers, Kind::Special, {{StatId::BodySearches, Op::Ge, 50}});
    add("GIBBON", kAllTiers, Kind::Special, {{StatId::HoldUps, Op::Ge, 50}});
    add("TORTOISE", kAllTiers, Kind::Special, {{StatId::BoxTimeMinutes, Op::Ge, 60}});
    add("RABBIT", kAllTiers, Kind::Special, {{StatId::MagazinePages, Op::Ge, 100}});
    add("BEE", kAllTiers, Kind::Special, {{StatId::SyringeUses, Op::Ge, 50}});
    add("GECKO", kAllTiers, Kind::Special, {{StatId::WallTimeMinutes, Op::Ge, 60}});
    add("SCARAB", kAllTiers, Kind::Special, {{StatId::SideRolls, Op::Ge, 100}});
    add("FROG", kAllTiers, Kind::Special, {{StatId::ForwardRolls, Op::Ge, 200}});
    add("INCH WORM", kAllTiers, Kind::Special, {{StatId::CrawlTimeMinutes, Op::Ge, 60}});
    add("LOBSTER", kAllTiers, Kind::Special, {{StatId::CrouchTimeMinutes, Op::Ge, 150}});
    add("HYENA", kAllTiers, Kind::Special, {{StatId::Pickups, Op::Ge, 400}});
    add("HOG", kAllTiers, Kind::Special, {{StatId::CombatHighs, Op::Ge, 10}});
    add("PIG", kAllTiers, Kind::Special, {{StatId::RationsUsed, Op::Ge, 40}});
    add("COW", kAllTiers, Kind::Special, {{StatId::Alerts, Op::Ge, 100}});
    add("CROCODILE", kAllTiers, Kind::Special, {{StatId::Kills, Op::Ge, 400}});
    add("GIANT PANDA", kAllTiers, Kind::Special, {{StatId::PlayTimeHours, Op::Ge, 30}}, true);

    struct GridRow { const char* name; bool high_alerts; bool high_kills; bool high_continues; };
    constexpr GridRow grid[] = {
        {"SCORPION", false, false, false}, {"TARANTULA", false, true, false},
        {"CENTIPEDE", false, false, true}, {"SPIDER", false, true, true},
        {"JAGUAR", true, false, false},    {"PANTHER", true, true, false},
        {"LEOPARD", true, false, true},   {"PUMA", true, true, true},
    };
    for (const GridRow& row : grid) {
        add(row.name, kAllTiers, Kind::Regular,
            {{StatId::Alerts, row.high_alerts ? Op::Gt : Op::Le, 75},
             {StatId::Kills, row.high_kills ? Op::Gt : Op::Le, 250},
             {StatId::Continues, row.high_continues ? Op::Gt : Op::Le, 25}});
    }

    add("CHICKEN", kAllTiers, Kind::Worst,
        {{StatId::Alerts, Op::Ge, 150}, {StatId::Kills, Op::Ge, 500},
         {StatId::Continues, Op::Ge, 50}, {StatId::RationsUsed, Op::Ge, 50},
         {StatId::PlayTimeHours, Op::Ge, 35}}, true);
    return rules;
}

struct ReqRow { const char* label; StatId stat; Op op; double limit; ReqFmt fmt; };
constexpr std::array<ReqRow, 6> kBigBossReqs{{
    {"alerts", StatId::Alerts, Op::Eq, 0, ReqFmt::Count},
    {"kills", StatId::Kills, Op::Eq, 0, ReqFmt::Count},
    {"continues", StatId::Continues, Op::Eq, 0, ReqFmt::Count},
    {"recovery items", StatId::RationsUsed, Op::Eq, 0, ReqFmt::Count},
    {"play time", StatId::PlayTimeHours, Op::Le, 5, ReqFmt::Time},
    {"special items", StatId::SpecialItemUsed, Op::Eq, 0, ReqFmt::Count},
}};

} // namespace

std::span<const RankRule> mgs4_rules()
{
    static std::vector<std::vector<Cond>> pool;
    static const std::vector<RankRule> rules = build_rules(pool);
    return rules;
}

std::optional<Match> evaluate_mgs4(const GameStats& s)
{
    for (const RankRule& rule : mgs4_rules()) {
        if (rule_matches(s, rule)) return Match{rule.name, rule.kind};
    }
    return std::nullopt;
}

std::vector<Match> all_matches_mgs4(const GameStats& s)
{
    std::vector<Match> matches;
    for (const RankRule& rule : mgs4_rules()) {
        if (rule_matches(s, rule)) matches.push_back({rule.name, rule.kind});
    }
    return matches;
}

std::vector<ReqStatus> elite_requirements_mgs4(const GameStats& s)
{
    std::vector<ReqStatus> out;
    for (const ReqRow& row : kBigBossReqs) {
        out.push_back({row.label, cond_met(s, {row.stat, row.op, row.limit}),
                       stat_value(s, row.stat), row.limit,
                       static_cast<uint8_t>(row.fmt), static_cast<uint8_t>(row.op)});
    }
    return out;
}

} // namespace bb::codename
