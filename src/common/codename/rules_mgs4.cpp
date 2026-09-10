#include "rules_mgs4.h"

#include "codename.h"

#include <array>
#include <utility>
#include <vector>

namespace bb::codename {
namespace {

constexpr TierMask kHardUp = kH | kX;
constexpr TierMask kSolidUp = kN | kHardUp;
constexpr TierMask kNakedUp = kE | kSolidUp;

// Single source for the BIG BOSS ladder: the requirements panel reads the
// rows, the rank matches on the derived conds.
constexpr std::array<ReqRow, 6> kBigBossRows{{
    {"alerts", StatId::Alerts, Op::Eq, 0, ReqFmt::Count},
    {"kills", StatId::Kills, Op::Eq, 0, ReqFmt::Count},
    {"continues", StatId::Continues, Op::Eq, 0, ReqFmt::Count},
    {"recovery items", StatId::RationsUsed, Op::Eq, 0, ReqFmt::Count},
    {"play time", StatId::PlayTimeHours, Op::Le, 5, ReqFmt::Time},
    {"special items", StatId::SpecialItemUsed, Op::Eq, 0, ReqFmt::Count},
}};

constexpr std::array<Cond, 6> kBigBossConds = conds_from_rows(kBigBossRows);

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
    // BIG BOSS matches on the shared ladder conds; the looser elites keep
    // their parameterized rows.
    add("BIG BOSS", kX, Kind::Elite,
        {kBigBossConds.begin(), kBigBossConds.end()}, true);
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

    add("BEAR", kAllTiers, Kind::Special, {{StatId::CqcChokes, Op::Ge, mgs4_goals::kBearChokes}});
    add("EAGLE", kAllTiers, Kind::Special, {{StatId::Headshots, Op::Ge, mgs4_goals::kEagleHeadshots}});
    add("ASSASSIN", kAllTiers, Kind::Special,
        {{StatId::KnifeDefeats, Op::Ge, mgs4_goals::kAssassinKnife},
         {StatId::CqcHolds, Op::Ge, mgs4_goals::kAssassinCqcHolds},
         {StatId::Alerts, Op::Le, mgs4_goals::kAssassinMaxAlerts}});
    add("PIGEON", kAllTiers, Kind::Special, {{StatId::Kills, Op::Eq, 0}});
    add("BLUE BIRD", kAllTiers, Kind::Special, {{StatId::ItemsGiven, Op::Ge, mgs4_goals::kBlueBirdItems}});
    add("HAWK", kAllTiers, Kind::Special, {{StatId::Praises, Op::Ge, mgs4_goals::kHawkPraises}});
    add("LITTLE GRAY", kAllTiers, Kind::Special, {{StatId::WeaponsAcquired, Op::Ge, mgs4_goals::kLittleGrayWeapons}});
    add("ANT", kAllTiers, Kind::Special, {{StatId::BodySearches, Op::Ge, mgs4_goals::kAntSearches}});
    add("GIBBON", kAllTiers, Kind::Special, {{StatId::HoldUps, Op::Ge, mgs4_goals::kGibbonHoldUps}});
    add("TORTOISE", kAllTiers, Kind::Special, {{StatId::BoxTimeMinutes, Op::Ge, mgs4_goals::kTortoiseBoxMinutes}});
    add("RABBIT", kAllTiers, Kind::Special, {{StatId::MagazinePages, Op::Ge, mgs4_goals::kRabbitPages}});
    add("BEE", kAllTiers, Kind::Special, {{StatId::SyringeUses, Op::Ge, mgs4_goals::kBeeSyringeUses}});
    add("GECKO", kAllTiers, Kind::Special, {{StatId::WallTimeMinutes, Op::Ge, mgs4_goals::kGeckoWallMinutes}});
    add("SCARAB", kAllTiers, Kind::Special, {{StatId::SideRolls, Op::Ge, mgs4_goals::kScarabSideRolls}});
    add("FROG", kAllTiers, Kind::Special, {{StatId::ForwardRolls, Op::Ge, mgs4_goals::kFrogForwardRolls}});
    add("INCH WORM", kAllTiers, Kind::Special, {{StatId::CrawlTimeMinutes, Op::Ge, mgs4_goals::kInchWormCrawlMinutes}});
    add("LOBSTER", kAllTiers, Kind::Special, {{StatId::CrouchTimeMinutes, Op::Ge, mgs4_goals::kLobsterCrouchMinutes}});
    add("HYENA", kAllTiers, Kind::Special, {{StatId::Pickups, Op::Ge, mgs4_goals::kHyenaPickups}});
    add("HOG", kAllTiers, Kind::Special, {{StatId::CombatHighs, Op::Ge, mgs4_goals::kHogCombatHighs}});
    add("PIG", kAllTiers, Kind::Special, {{StatId::RationsUsed, Op::Ge, mgs4_goals::kPigRations}});
    add("COW", kAllTiers, Kind::Special, {{StatId::Alerts, Op::Ge, mgs4_goals::kCowAlerts}});
    add("CROCODILE", kAllTiers, Kind::Special, {{StatId::Kills, Op::Ge, mgs4_goals::kCrocodileKills}});
    add("GIANT PANDA", kAllTiers, Kind::Special, {{StatId::PlayTimeHours, Op::Ge, mgs4_goals::kGiantPandaHours}}, true);

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
        {{StatId::Alerts, Op::Ge, mgs4_goals::kChickenAlerts},
         {StatId::Kills, Op::Ge, mgs4_goals::kChickenKills},
         {StatId::Continues, Op::Ge, mgs4_goals::kChickenContinues},
         {StatId::RationsUsed, Op::Ge, mgs4_goals::kChickenRecoveryItems},
         {StatId::PlayTimeHours, Op::Ge, mgs4_goals::kChickenHours}}, true);
    return rules;
}

} // namespace

std::span<const RankRule> mgs4_rules()
{
    static std::vector<std::vector<Cond>> pool;
    static const std::vector<RankRule> rules = build_rules(pool);
    return rules;
}

std::optional<Match> evaluate_mgs4(const GameStats& s)
{
    return first_match(s, mgs4_rules());
}

std::vector<Match> all_matches_mgs4(const GameStats& s)
{
    return all_matches(s, mgs4_rules());
}

std::vector<ReqStatus> elite_requirements_mgs4(const GameStats& s)
{
    return requirements_from_rows(s, kBigBossRows, false);
}

} // namespace bb::codename
