#include "rules_mgs2.h"

#include <array>
#include <utility>
#include <vector>

namespace bb::codename {
namespace {

constexpr TierMask kEE = 1u << 5;
constexpr TierMask kEV = kE | kVe;
constexpr TierMask kAllM2 = kVe | kE | kN | kH | kX | kEE;

constexpr int kMissionPlant = 0;
constexpr int kMissionTanker = 16;
constexpr int kMissionTP = 32;

Cond mission_cond(int mission)
{
    return Cond{StatId::MissionCode, Op::Eq, static_cast<double>(mission)};
}

struct Mgs2Tier {
    TierMask mask;
    const char* worst_name;
    const char* special_low_alerts;
    const char* special_fast;
    const char* special_kills;
    const char* special_meals;
    const char* special_time;
    const char* special_saves;
};

constexpr Mgs2Tier kTiers[4] = {
    {kX | kEE, "Ostrich", "Night Owl", "Eagle", "Orca", "Whale", "Giant Panda", "Hippopotamus"},
    {kH, "Rabbit", "Flying Fox", "Hawk", "Jaws", "Mammoth", "Sloth", "Zebra"},
    {kN, "Mouse", "Bat", "Falcon", "Shark", "Elephant", "Capybara", "Deer"},
    {kEV, "Chicken", "Flying Squirrel", "Swallow", "Piranha", "Pig", "Koala", "Cat"},
};

std::vector<Cond> elite_conds(int strictness)
{
    std::vector<Cond> c;
    c.push_back(mission_cond(kMissionTP));
    c.push_back({StatId::SpecialItemUsed, Op::Eq, 0});
    if (strictness == 0) {
        c.push_back({StatId::RadarOff, Op::Eq, 1});
        c.push_back({StatId::ShotsFired, Op::Le, 700});
        c.push_back({StatId::DamageBars, Op::Le, 10});
        c.push_back({StatId::PlayTimeMinutes, Op::Le, 180});
        c.push_back({StatId::Alerts, Op::Le, 3});
        c.push_back({StatId::Kills, Op::Eq, 0});
        c.push_back({StatId::RationsUsed, Op::Eq, 0});
        c.push_back({StatId::Continues, Op::Eq, 0});
        c.push_back({StatId::Saves, Op::Le, 8});
    } else if (strictness == 1) {
        c.push_back({StatId::Alerts, Op::Le, 3});
        c.push_back({StatId::Kills, Op::Eq, 0});
        c.push_back({StatId::RationsUsed, Op::Eq, 0});
        c.push_back({StatId::PlayTimeMinutes, Op::Le, 180});
        c.push_back({StatId::Continues, Op::Eq, 0});
        c.push_back({StatId::Saves, Op::Le, 16});
    } else if (strictness == 2) {
        c.push_back({StatId::Alerts, Op::Le, 4});
        c.push_back({StatId::Kills, Op::Eq, 0});
        c.push_back({StatId::RationsUsed, Op::Le, 3});
        c.push_back({StatId::PlayTimeMinutes, Op::Le, 195});
        c.push_back({StatId::Continues, Op::Eq, 0});
    } else {
        c.push_back({StatId::Alerts, Op::Le, 5});
        c.push_back({StatId::Kills, Op::Eq, 0});
        c.push_back({StatId::PlayTimeMinutes, Op::Le, 210});
        c.push_back({StatId::Continues, Op::Eq, 0});
    }
    return c;
}

struct GridRow {
    const char* name;
    double amin;
    double amax;
    double cmin;
    double cmax;
    double kmin;
    double kmax;
};

constexpr GridRow kGridTanker[] = {
    {"Scorpion", 1, 5, 0, 10, 1, 15},
    {"Tarantula", 1, 5, 0, 10, 16, 49},
    {"Centipede", 1, 5, 11, -1, 1, 15},
    {"Spider", 1, 5, 11, -1, 16, 49},
    {"Jaguar", 6, 15, 0, 10, 1, 15},
    {"Panther", 6, 15, 0, 10, 16, 49},
    {"Leopard", 6, 15, 11, -1, 1, 15},
    {"Puma", 6, 15, 11, -1, 16, 49},
    {"Jackal", 16, 30, 0, 10, 1, 15},
    {"Tasmanian Devil", 16, 30, 0, 10, 16, 49},
    {"Mongoose", 16, 30, 11, -1, 1, 15},
    {"Hyena", 16, 30, 11, -1, 16, 49},
    {"Iguana", 31, 49, 0, 10, 1, 15},
    {"Crocodile", 31, 49, 0, 10, 16, 49},
    {"KOMODO DRAGON", 31, 49, 11, -1, 1, 15},
    {"Alligator", 31, 49, 11, -1, 16, 49},
};
constexpr GridRow kGridPlant[] = {
    {"Scorpion", 1, 15, 0, 30, 1, 60},
    {"Tarantula", 1, 15, 0, 30, 61, 199},
    {"Centipede", 1, 15, 31, -1, 1, 60},
    {"Spider", 1, 15, 31, -1, 61, 199},
    {"Jaguar", 16, 40, 0, 30, 1, 60},
    {"Panther", 16, 40, 0, 30, 61, 199},
    {"Leopard", 16, 40, 31, -1, 1, 60},
    {"Puma", 16, 40, 31, -1, 61, 199},
    {"Jackal", 41, 70, 0, 30, 1, 60},
    {"Tasmanian Devil", 41, 70, 0, 30, 61, 199},
    {"Mongoose", 41, 70, 31, -1, 1, 60},
    {"Hyena", 41, 70, 31, -1, 61, 199},
    {"Iguana", 71, 199, 0, 30, 1, 60},
    {"Crocodile", 71, 199, 0, 30, 61, 199},
    {"KOMODO DRAGON", 71, 199, 31, -1, 1, 60},
    {"Alligator", 71, 199, 31, -1, 61, 199},
};
constexpr GridRow kGridTP[] = {
    {"Scorpion", 1, 20, 0, 40, 1, 70},
    {"Tarantula", 1, 20, 0, 40, 71, 249},
    {"Centipede", 1, 20, 41, -1, 1, 70},
    {"Spider", 1, 20, 41, -1, 71, 249},
    {"Jaguar", 21, 50, 0, 40, 1, 70},
    {"Panther", 21, 50, 0, 40, 71, 249},
    {"Leopard", 21, 50, 41, -1, 1, 70},
    {"Puma", 21, 50, 41, -1, 71, 249},
    {"Jackal", 51, 80, 0, 40, 1, 70},
    {"Tasmanian Devil", 51, 80, 0, 40, 71, 249},
    {"Mongoose", 51, 80, 41, -1, 1, 70},
    {"Hyena", 51, 80, 41, -1, 71, 249},
    {"Iguana", 81, 249, 0, 40, 1, 70},
    {"Crocodile", 81, 249, 0, 40, 71, 249},
    {"KOMODO DRAGON", 81, 249, 41, -1, 1, 70},
    {"Alligator", 81, 249, 41, -1, 71, 249},
};

std::vector<RankRule> build_rules(std::vector<std::vector<Cond>>& cond_pool)
{
    std::vector<RankRule> rules;

    auto add = [&cond_pool, &rules](const char* name, TierMask tiers, Kind kind,
                                    std::vector<Cond> conds) {
        cond_pool.push_back(std::move(conds));
        rules.push_back(RankRule{name, tiers, kind, cond_pool.back()});
    };

    static constexpr const char* kLadder[] = {"BIG BOSS", "FOX", "DOBERMAN", "HOUND"};
    static constexpr std::pair<TierMask, int> kEliteTiers[] = {
        {kX | kEE, 0}, {kH, 1}, {kN, 2}, {kE, 3}};
    for (const auto& [tier, offset] : kEliteTiers) {
        for (int strictness = 0; strictness + offset < 4; ++strictness) {
            add(kLadder[strictness + offset], tier, Kind::Elite, elite_conds(strictness));
        }
    }

    auto add_worst = [&](const Mgs2Tier& tier) {
        add(tier.worst_name, tier.mask, Kind::Worst,
            {mission_cond(kMissionTP),
             {StatId::Alerts, Op::Ge, 250},
             {StatId::Kills, Op::Ge, 250},
             {StatId::RationsUsed, Op::Ge, 31},
             {StatId::PlayTimeMinutes, Op::Ge, 1800},
             {StatId::Continues, Op::Ge, 60},
             {StatId::Saves, Op::Ge, 100}});
    };
    for (const Mgs2Tier& tier : kTiers) {
        add_worst(tier);
    }

    add("SEA LOUSE", kAllM2, Kind::Special,
        {{StatId::SeaLouse, Op::Eq, 1}, mission_cond(kMissionPlant)});
    add("SEA LOUSE", kAllM2, Kind::Special,
        {{StatId::SeaLouse, Op::Eq, 1}, mission_cond(kMissionTP)});

    struct MissionThresholds {
        double tanker;
        double plant;
        double tp;
    };
    auto per_mission = [&](const char* name, Kind kind, StatId stat, Op op,
                           const MissionThresholds& th) {
        const std::pair<int, double> ms[] = {{kMissionTanker, th.tanker},
                                             {kMissionPlant, th.plant},
                                             {kMissionTP, th.tp}};
        for (const auto& [mission, limit] : ms) {
            add(name, kAllM2, kind,
                {mission_cond(mission), Cond{stat, op, limit}});
        }
    };

    for (const Mgs2Tier& tier : kTiers) {
        add(tier.special_low_alerts, tier.mask, Kind::Special,
            {{StatId::Alerts, Op::Le, 0}, mission_cond(kMissionTanker)});
        add(tier.special_low_alerts, tier.mask, Kind::Special,
            {{StatId::Alerts, Op::Le, 3}, mission_cond(kMissionPlant)});
        add(tier.special_low_alerts, tier.mask, Kind::Special,
            {{StatId::Alerts, Op::Le, 3}, mission_cond(kMissionTP)});
    }

    add("PIGEON", kAllM2, Kind::Special, {{StatId::Kills, Op::Eq, 0}});

    for (const Mgs2Tier& tier : kTiers) {
        add(tier.special_fast, tier.mask, Kind::Special,
            {{StatId::PlayTimeMinutes, Op::Le, 18}, mission_cond(kMissionTanker)});
        add(tier.special_fast, tier.mask, Kind::Special,
            {{StatId::PlayTimeMinutes, Op::Le, 165}, mission_cond(kMissionPlant)});
        add(tier.special_fast, tier.mask, Kind::Special,
            {{StatId::PlayTimeMinutes, Op::Le, 180}, mission_cond(kMissionTP)});
    }

    per_mission("GAZELLE", Kind::Special, StatId::ClearingEscapes, Op::Ge,
                {50, 100, 150});

    per_mission("Cow", Kind::Special, StatId::Alerts, Op::Ge, {50, 200, 250});

    for (const Mgs2Tier& tier : kTiers) {
        add(tier.special_kills, tier.mask, Kind::Special,
            {{StatId::Kills, Op::Ge, 50}, mission_cond(kMissionTanker)});
        add(tier.special_kills, tier.mask, Kind::Special,
            {{StatId::Kills, Op::Ge, 200}, mission_cond(kMissionPlant)});
        add(tier.special_kills, tier.mask, Kind::Special,
            {{StatId::Kills, Op::Ge, 250}, mission_cond(kMissionTP)});
    }

    for (const Mgs2Tier& tier : kTiers) {
        add(tier.special_meals, tier.mask, Kind::Special, {{StatId::RationsUsed, Op::Ge, 31}});
    }

    for (const Mgs2Tier& tier : kTiers) {
        add(tier.special_time, tier.mask, Kind::Special,
            {{StatId::PlayTimeMinutes, Op::Ge, 300}, mission_cond(kMissionTanker)});
        add(tier.special_time, tier.mask, Kind::Special,
            {{StatId::PlayTimeMinutes, Op::Ge, 1500}, mission_cond(kMissionPlant)});
        add(tier.special_time, tier.mask, Kind::Special,
            {{StatId::PlayTimeMinutes, Op::Ge, 1800}, mission_cond(kMissionTP)});
    }

    for (const Mgs2Tier& tier : kTiers) {
        add(tier.special_saves, tier.mask, Kind::Special,
            {{StatId::Saves, Op::Ge, 25}, mission_cond(kMissionTanker)});
        add(tier.special_saves, tier.mask, Kind::Special,
            {{StatId::Saves, Op::Ge, 75}, mission_cond(kMissionPlant)});
        add(tier.special_saves, tier.mask, Kind::Special,
            {{StatId::Saves, Op::Ge, 100}, mission_cond(kMissionTP)});
    }

    auto push_grid = [&](const GridRow* rows, size_t n, int mission) {
        for (size_t i = 0; i < n; ++i) {
            const GridRow& r = rows[i];
            std::vector<Cond> c{{StatId::MissionCode, Op::Eq, static_cast<double>(mission)},
                                {StatId::Kills, Op::Ge, r.kmin},
                                {StatId::Kills, Op::Le, r.kmax},
                                {StatId::Alerts, Op::Ge, r.amin}};
            if (r.amax > 0) {
                c.push_back({StatId::Alerts, Op::Le, r.amax});
            }
            if (r.cmax > 0) {
                c.push_back({StatId::Continues, Op::Le, r.cmax});
            } else {
                c.push_back({StatId::Continues, Op::Ge, r.cmin});
            }
            add(r.name, kAllM2, Kind::Regular, c);
        }
    };
    push_grid(kGridTanker, std::size(kGridTanker), kMissionTanker);
    push_grid(kGridPlant, std::size(kGridPlant), kMissionPlant);
    push_grid(kGridTP, std::size(kGridTP), kMissionTP);

    return rules;
}

} // namespace

std::span<const RankRule> mgs2_rules()
{
    static std::vector<std::vector<Cond>> cond_pool;
    static const std::vector<RankRule> kRules = build_rules(cond_pool);
    return kRules;
}

}
