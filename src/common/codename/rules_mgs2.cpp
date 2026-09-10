#include "codename.h"

#include <array>

namespace bb::codename {
namespace {

constexpr int kMissionPlant = 0;
constexpr int kMissionTanker = 16;
constexpr int kMissionTP = 32;

constexpr std::array<ReqRow, 11> kEliteLadder{{
    {"campaign", StatId::MissionCode, Op::Eq, 32, ReqFmt::Count},
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

struct Tier {
    const char* worst;
    const char* low_alerts;
    const char* fast;
    const char* kills;
    const char* meals;
    const char* time;
    const char* saves;
};

constexpr Tier kTiers[] = {
    {kAnimalTiers.worst.top, kAnimalTiers.low_alerts.top, kAnimalTiers.fast.top,
     kAnimalTiers.kills.top, kAnimalTiers.meals.top, kAnimalTiers.time.top,
     kAnimalTiers.saves.top},
    {kAnimalTiers.worst.high, kAnimalTiers.low_alerts.high, kAnimalTiers.fast.high,
     kAnimalTiers.kills.high, kAnimalTiers.meals.high, kAnimalTiers.time.high,
     kAnimalTiers.saves.high},
    {kAnimalTiers.worst.normal, kAnimalTiers.low_alerts.normal, kAnimalTiers.fast.normal,
     kAnimalTiers.kills.normal, kAnimalTiers.meals.normal, kAnimalTiers.time.normal,
     kAnimalTiers.saves.normal},
    {kAnimalTiers.worst.low, kAnimalTiers.low_alerts.low, kAnimalTiers.fast.low,
     kAnimalTiers.kills.low, kAnimalTiers.meals.low, kAnimalTiers.time.low,
     kAnimalTiers.saves.low},
};

struct GridRow {
    const char* name;
    int amin, amax, cmin, cmax, kmin, kmax;
};

constexpr GridRow kGridTanker[] = {
    {"Scorpion", 1, 5, 0, 10, 1, 15}, {"Tarantula", 1, 5, 0, 10, 16, 49},
    {"Centipede", 1, 5, 11, -1, 1, 15}, {"Spider", 1, 5, 11, -1, 16, 49},
    {"Jaguar", 6, 15, 0, 10, 1, 15}, {"Panther", 6, 15, 0, 10, 16, 49},
    {"Leopard", 6, 15, 11, -1, 1, 15}, {"Puma", 6, 15, 11, -1, 16, 49},
    {"Jackal", 16, 30, 0, 10, 1, 15}, {"Tasmanian Devil", 16, 30, 0, 10, 16, 49},
    {"Mongoose", 16, 30, 11, -1, 1, 15}, {"Hyena", 16, 30, 11, -1, 16, 49},
    {"Iguana", 31, 49, 0, 10, 1, 15}, {"Crocodile", 31, 49, 0, 10, 16, 49},
    {"KOMODO DRAGON", 31, 49, 11, -1, 1, 15}, {"Alligator", 31, 49, 11, -1, 16, 49},
};
constexpr GridRow kGridPlant[] = {
    {"Scorpion", 1, 15, 0, 30, 1, 60}, {"Tarantula", 1, 15, 0, 30, 61, 199},
    {"Centipede", 1, 15, 31, -1, 1, 60}, {"Spider", 1, 15, 31, -1, 61, 199},
    {"Jaguar", 16, 40, 0, 30, 1, 60}, {"Panther", 16, 40, 0, 30, 61, 199},
    {"Leopard", 16, 40, 31, -1, 1, 60}, {"Puma", 16, 40, 31, -1, 61, 199},
    {"Jackal", 41, 70, 0, 30, 1, 60}, {"Tasmanian Devil", 41, 70, 0, 30, 61, 199},
    {"Mongoose", 41, 70, 31, -1, 1, 60}, {"Hyena", 41, 70, 31, -1, 61, 199},
    {"Iguana", 71, 199, 0, 30, 1, 60}, {"Crocodile", 71, 199, 0, 30, 61, 199},
    {"KOMODO DRAGON", 71, 199, 31, -1, 1, 60}, {"Alligator", 71, 199, 31, -1, 61, 199},
};
constexpr GridRow kGridTP[] = {
    {"Scorpion", 1, 20, 0, 40, 1, 70}, {"Tarantula", 1, 20, 0, 40, 71, 249},
    {"Centipede", 1, 20, 41, -1, 1, 70}, {"Spider", 1, 20, 41, -1, 71, 249},
    {"Jaguar", 21, 50, 0, 40, 1, 70}, {"Panther", 21, 50, 0, 40, 71, 249},
    {"Leopard", 21, 50, 41, -1, 1, 70}, {"Puma", 21, 50, 41, -1, 71, 249},
    {"Jackal", 51, 80, 0, 40, 1, 70}, {"Tasmanian Devil", 51, 80, 0, 40, 71, 249},
    {"Mongoose", 51, 80, 41, -1, 1, 70}, {"Hyena", 51, 80, 41, -1, 71, 249},
    {"Iguana", 81, 249, 0, 40, 1, 70}, {"Crocodile", 81, 249, 0, 40, 71, 249},
    {"KOMODO DRAGON", 81, 249, 41, -1, 1, 70}, {"Alligator", 81, 249, 41, -1, 71, 249},
};

int tier_index(Difficulty difficulty)
{
    if (difficulty == Difficulty::Extreme || difficulty == Difficulty::EuroExtreme) return 0;
    if (difficulty == Difficulty::Hard) return 1;
    if (difficulty == Difficulty::Normal) return 2;
    return 3;
}

double mission_limit(int mission, double tanker, double plant, double tp)
{
    if (mission == kMissionTanker) return tanker;
    if (mission == kMissionPlant) return plant;
    if (mission == kMissionTP) return tp;
    return -1;
}

bool elite_match(const GameStats& s, int strictness)
{
    if (s.mission != kMissionTP || s.special_item_used) return false;
    const double minutes = stat_value(s, StatId::PlayTimeMinutes);
    if (strictness == 0) {
        return s.radar_off && s.shots_fired <= 700 && s.alerts <= 3
            && stat_value(s, StatId::DamageBars) <= 10 && s.kills == 0
            && s.rations_used == 0 && s.play_time_seconds <= 3 * 3600
            && s.continues == 0 && s.saves <= 8;
    }
    if (strictness == 1) return s.alerts <= 3 && s.kills == 0 && s.rations_used == 0
        && minutes <= 180 && s.continues == 0 && s.saves <= 16;
    if (strictness == 2) return s.alerts <= 4 && s.kills == 0 && s.rations_used <= 3
        && minutes <= 195 && s.continues == 0;
    return s.alerts <= 5 && s.kills == 0 && minutes <= 210 && s.continues == 0;
}

const char* grid_match(const GameStats& s)
{
    std::span<const GridRow> grid;
    if (s.mission == kMissionTanker) grid = kGridTanker;
    else if (s.mission == kMissionPlant) grid = kGridPlant;
    else if (s.mission == kMissionTP) grid = kGridTP;
    else return nullptr;
    for (const GridRow& row : grid) {
        if (s.kills < row.kmin || s.kills > row.kmax || s.alerts < row.amin
            || (row.amax > 0 && s.alerts > row.amax)
            || (row.cmax > 0 ? s.continues > row.cmax : s.continues < row.cmin)) continue;
        return row.name;
    }
    return nullptr;
}

} // namespace

std::optional<Match> evaluate_mgs2(const GameStats& s)
{
    static constexpr const char* kEliteNames[] = {"BIG BOSS", "FOX", "DOBERMAN", "HOUND"};
    const int tier = tier_index(s.difficulty);
    const int elite_offset = s.difficulty == Difficulty::VeryEasy ? 4 : tier;
    for (int strictness = 0; strictness + elite_offset < 4; ++strictness) {
        if (elite_match(s, strictness))
            return Match{kEliteNames[strictness + elite_offset], Kind::Elite};
    }

    const Tier& names = kTiers[tier];
    const double minutes = stat_value(s, StatId::PlayTimeMinutes);
    if (s.mission == kMissionTP && s.alerts >= 250 && s.kills >= 250
        && s.rations_used >= 31 && minutes >= 1800 && s.continues >= 60
        && s.saves >= 100) return Match{names.worst, Kind::Worst};
    if (s.sea_louse && (s.mission == kMissionPlant || s.mission == kMissionTP))
        return Match{"SEA LOUSE", Kind::Special};
    const double low_alerts = mission_limit(s.mission, 0, 3, 3);
    if (low_alerts >= 0 && s.alerts <= low_alerts) return Match{names.low_alerts, Kind::Special};
    if (s.kills == 0) return Match{"PIGEON", Kind::Special};
    const double fast = mission_limit(s.mission, 18, 165, 180);
    if (fast >= 0 && minutes <= fast) return Match{names.fast, Kind::Special};
    const double escapes = mission_limit(s.mission, 50, 100, 150);
    if (escapes >= 0 && s.clearing_escapes >= escapes) return Match{"GAZELLE", Kind::Special};
    const double cow = mission_limit(s.mission, 50, 200, 250);
    if (cow >= 0 && s.alerts >= cow) return Match{"Cow", Kind::Special};
    const double kills = mission_limit(s.mission, 50, 200, 250);
    if (kills >= 0 && s.kills >= kills) return Match{names.kills, Kind::Special};
    if (s.rations_used >= 31) return Match{names.meals, Kind::Special};
    const double slow = mission_limit(s.mission, 300, 1500, 1800);
    if (slow >= 0 && minutes >= slow) return Match{names.time, Kind::Special};
    const double saves = mission_limit(s.mission, 25, 75, 100);
    if (saves >= 0 && s.saves >= saves) return Match{names.saves, Kind::Special};
    if (const char* name = grid_match(s)) return Match{name, Kind::Regular};
    return std::nullopt;
}

std::vector<ReqStatus> elite_requirements_mgs2(const GameStats& s)
{
    return requirements_from_rows(s, kEliteLadder, false);
}

} // namespace bb::codename
