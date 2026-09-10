#include <cstdio>
#include <string_view>

#include "check.h"
#include "common/codename/codename.h"
#include "common/stats.h"
#include "games/mgs2/dog_tags.h"

using namespace bb;
using namespace bb::codename;

namespace {

GameStats base(Difficulty d, int mission)
{
    GameStats s{};
    s.difficulty = d;
    s.mission = mission;
    return s;
}

const char* best(const GameStats& s)
{
    auto m = evaluate_mgs2(s);
    return m ? m->name : "<none>";
}

void test_dog_tag_bits_and_rosters()
{
    uint32_t flags[mgs2::kDogTagWordCount]{};
    flags[0] = (1u << 0) | (1u << 31);
    flags[31] = 1u << 31;
    CHECK(mgs2::dog_tag_count(flags) == 3);
    CHECK(mgs2::dog_tag_collected(flags, 0));
    CHECK(mgs2::dog_tag_collected(flags, 31));
    CHECK(mgs2::dog_tag_collected(flags, 1023));
    CHECK(!mgs2::dog_tag_collected(flags, 1024));

    CHECK(mgs2::dog_tag_available(mgs2::kDogTags[0], 16, 0));
    CHECK(!mgs2::dog_tag_available(mgs2::kDogTags[0], 0, 0));
    CHECK(mgs2::dog_tag_available(mgs2::kDogTags[4], 32, 5));

    CHECK(std::string_view(mgs2::kDogTagAreas[0]) == "w00a");
    CHECK(std::string_view(mgs2::kDogTagAreas[33]) == "w43a");
    for (const auto& tag : mgs2::kDogTags) {
        bool grouped = false;
        for (const char* area : mgs2::kDogTagAreas) {
            grouped |= std::string_view(tag.area) == area;
        }
        CHECK(grouped);
    }
}

void test_big_boss_exact()
{
    GameStats s = base(Difficulty::Extreme, 32);
    s.radar_off = true;
    s.shots_fired = 700;
    s.alerts = 3;
    s.damage_taken_bars = 10;
    s.play_time_seconds = 180 * 60;
    s.saves = 8;
    CHECK(std::string_view(best(s)) == "BIG BOSS");

    s.damage_taken_bars = 11;
    CHECK(std::string_view(best(s)) == "FOX");
}

void test_bb_blocked_by_radar_and_mission()
{
    GameStats s = base(Difficulty::Extreme, 32);
    s.shots_fired = 699;
    s.saves = 8;
    CHECK(std::string_view(best(s)) == "FOX");

    GameStats t = base(Difficulty::Extreme, 16);
    t.shots_fired = 699;
    t.saves = 8;
    CHECK(std::string_view(best(t)) == "Night Owl");
}

void test_big_boss_requirements_include_story_selection()
{
    GameStats s = base(Difficulty::Extreme, 16);
    s.radar_off = true;
    const auto requirements = elite_requirements_mgs2(s);
    CHECK(requirements.size() == 11);
    CHECK(std::string_view(requirements.front().label) == "campaign");
    CHECK(!requirements.front().pass);

    s.mission = 32;
    CHECK(elite_requirements_mgs2(s).front().pass);
}

void test_fox_extreme_loose()
{
    GameStats s = base(Difficulty::Extreme, 32);
    s.alerts = 3;
    s.saves = 16;
    CHECK(std::string_view(best(s)) == "FOX");
}

void test_euro_extreme_matches_extreme()
{
    GameStats s = base(Difficulty::EuroExtreme, 32);
    s.radar_off = true;
    s.shots_fired = 700;
    s.alerts = 3;
    s.damage_taken_bars = 10;
    s.play_time_seconds = 180 * 60;
    s.saves = 8;
    CHECK(std::string_view(best(s)) == "BIG BOSS");

    GameStats t = base(Difficulty::EuroExtreme, 32);
    t.alerts = 3;
    t.saves = 16;
    CHECK(std::string_view(best(t)) == "FOX");
}

void test_elite_ladder_by_difficulty()
{
    GameStats s = base(Difficulty::Hard, 32);
    s.radar_off = true;
    s.shots_fired = 100;
    s.damage_taken_bars = 1;
    s.play_time_seconds = 3600.0 * 2;
    s.saves = 8;
    CHECK(std::string_view(best(s)) == "FOX");

    s.difficulty = Difficulty::Normal;
    CHECK(std::string_view(best(s)) == "DOBERMAN");

    s.difficulty = Difficulty::Easy;
    CHECK(std::string_view(best(s)) == "HOUND");

    s.difficulty = Difficulty::VeryEasy;
    CHECK(std::string_view(best(s)) != "HOUND");
}

void test_pigeon_zero_kills()
{
    GameStats s = base(Difficulty::Hard, 0);
    s.alerts = 10;
    CHECK(std::string_view(best(s)) == "PIGEON");
}

void test_worst_chicken_normal()
{
    GameStats s = base(Difficulty::Normal, 32);
    s.alerts = 250;
    s.kills = 250;
    s.rations_used = 31;
    s.continues = 60;
    s.saves = 100;
    s.play_time_seconds = 1799 * 60 + 1;
    CHECK(std::string_view(best(s)) == "Mouse");

    s.mission = 0;
    CHECK(std::string_view(best(s)) != "Mouse");
}

void test_swallow_tanker_fast_ve()
{
    GameStats s = base(Difficulty::VeryEasy, 16);
    s.kills = 5;
    s.alerts = 5;
    s.play_time_seconds = 60.0 * 17;
    CHECK(std::string_view(best(s)) == "Swallow");

    GameStats t = base(Difficulty::VeryEasy, 16);
    t.play_time_seconds = 60.0 * 17;
    CHECK(std::string_view(best(t)) == "Flying Squirrel");
}

void test_cow_alerts_by_mission()
{
    GameStats s = base(Difficulty::VeryEasy, 16);
    s.kills = 5;
    s.alerts = 50;
    s.play_time_seconds = 3600.0 * 6;
    CHECK(std::string_view(best(s)) == "Cow");

    GameStats t = base(Difficulty::VeryEasy, 32);
    t.kills = 5;
    t.alerts = 51;
    t.play_time_seconds = 3600.0 * 6;
    CHECK(std::string_view(best(t)) != "Cow");
}

void test_source_threshold_boundaries()
{
    GameStats s = base(Difficulty::Normal, 16);
    s.kills = 50;
    s.alerts = 10;
    s.play_time_seconds = 19 * 60;
    CHECK(std::string_view(best(s)) == "Shark");

    s.kills = 1;
    s.rations_used = 31;
    CHECK(std::string_view(best(s)) == "Elephant");

    s.rations_used = 0;
    s.play_time_seconds = 299 * 60 + 1;
    CHECK(std::string_view(best(s)) == "Capybara");

    s.play_time_seconds = 19 * 60;
    s.saves = 25;
    CHECK(std::string_view(best(s)) == "Deer");
}

void test_sea_louse_non_tanker_only()
{
    GameStats s = base(Difficulty::Normal, 0);
    s.kills = 1;
    s.alerts = 10;
    s.play_time_seconds = 3600.0 * 4;
    s.sea_louse = true;
    CHECK(std::string_view(best(s)) == "SEA LOUSE");

    s.mission = 32;
    CHECK(std::string_view(best(s)) == "SEA LOUSE");

    s.mission = 16;
    CHECK(std::string_view(best(s)) != "SEA LOUSE");
}

void test_gazelle_thresholds()
{
    GameStats s = base(Difficulty::Normal, 16);
    s.kills = 1;
    s.alerts = 10;
    s.play_time_seconds = 3600.0 * 4;
    s.clearing_escapes = 49;
    CHECK(std::string_view(best(s)) != "GAZELLE");
    s.clearing_escapes = 50;
    CHECK(std::string_view(best(s)) == "GAZELLE");

    s.mission = 0;
    s.clearing_escapes = 100;
    CHECK(std::string_view(best(s)) == "GAZELLE");

    s.mission = 32;
    s.clearing_escapes = 149;
    CHECK(std::string_view(best(s)) != "GAZELLE");
    s.clearing_escapes = 150;
    CHECK(std::string_view(best(s)) == "GAZELLE");
}

void test_regular_tanker_scorpion()
{
    GameStats s = base(Difficulty::Normal, 16);
    s.kills = 5;
    s.alerts = 3;
    s.continues = 2;
    s.play_time_seconds = 3600.0;
    CHECK(std::string_view(best(s)) == "Scorpion");

    s.alerts = 20;
    CHECK(std::string_view(best(s)) == "Jackal");

    s.alerts = 3;
    s.kills = 16;
    CHECK(std::string_view(best(s)) == "Tarantula");

    s.continues = 11;
    CHECK(std::string_view(best(s)) == "Spider");
}

void test_regular_tp_grid_dimensions()
{
    GameStats s = base(Difficulty::Normal, 32);
    s.alerts = 21;
    s.kills = 71;
    s.continues = 41;
    s.play_time_seconds = 3600.0 * 4;
    CHECK(std::string_view(best(s)) == "Puma");

    s.alerts = 81;
    s.kills = 1;
    CHECK(std::string_view(best(s)) == "KOMODO DRAGON");
}

void test_animal_tier_names()
{
    // Every special family awards the shared table's names at each tier.
    // Base stats isolate one family: Tanker mission, off the elite ladder
    // (wrong mission), off PIGEON (one kill), off the fast board (one hour).
    const Difficulty tiers[4] = {Difficulty::Extreme, Difficulty::Hard,
                                 Difficulty::Normal, Difficulty::Easy};
    struct Family {
        const char* names[4]; // Extreme, Hard, Normal, Easy
        GameStats stats;
    };
    auto tanker = [] {
        GameStats s{};
        s.mission = 16;
        s.alerts = 1;
        s.kills = 1;
        s.play_time_seconds = 3600.0;
        return s;
    };
    GameStats low_alerts = tanker();
    low_alerts.alerts = 0;
    low_alerts.kills = 0;
    GameStats fast = tanker();
    fast.play_time_seconds = 600.0;
    GameStats kills = tanker();
    kills.kills = 50;
    GameStats meals = tanker();
    meals.rations_used = 31;
    GameStats time = tanker();
    time.play_time_seconds = 300.0 * 60.0;
    GameStats saves = tanker();
    saves.saves = 25;
    GameStats worst{};
    worst.mission = 32;
    worst.alerts = 250;
    worst.kills = 250;
    worst.rations_used = 31;
    worst.play_time_seconds = 1800.0 * 60.0;
    worst.continues = 60;
    worst.saves = 100;
    const Family families[] = {
        {{"Ostrich", "Rabbit", "Mouse", "Chicken"}, worst},
        {{"Night Owl", "Flying Fox", "Bat", "Flying Squirrel"}, low_alerts},
        {{"Eagle", "Hawk", "Falcon", "Swallow"}, fast},
        {{"Orca", "Jaws", "Shark", "Piranha"}, kills},
        {{"Whale", "Mammoth", "Elephant", "Pig"}, meals},
        {{"Giant Panda", "Sloth", "Capybara", "Koala"}, time},
        {{"Hippopotamus", "Zebra", "Deer", "Cat"}, saves},
    };
    for (const Family& f : families) {
        for (int i = 0; i < 4; ++i) {
            GameStats s = f.stats;
            s.difficulty = tiers[i];
            CHECK(std::string_view(best(s)) == f.names[i]);
        }
    }
}

} // namespace

int main()
{
    constexpr bb::test::Case tests[] = {
        {"dog_tag_bits_and_rosters", test_dog_tag_bits_and_rosters},
        {"big_boss_exact", test_big_boss_exact},
        {"bb_blocked_by_radar_and_mission", test_bb_blocked_by_radar_and_mission},
        {"big_boss_requirements_include_story_selection",
         test_big_boss_requirements_include_story_selection},
        {"fox_extreme_loose", test_fox_extreme_loose},
        {"euro_extreme_matches_extreme", test_euro_extreme_matches_extreme},
        {"elite_ladder_by_difficulty", test_elite_ladder_by_difficulty},
        {"pigeon_zero_kills", test_pigeon_zero_kills},
        {"worst_chicken_normal", test_worst_chicken_normal},
        {"swallow_tanker_fast_ve", test_swallow_tanker_fast_ve},
        {"cow_alerts_by_mission", test_cow_alerts_by_mission},
        {"source_threshold_boundaries", test_source_threshold_boundaries},
        {"sea_louse_non_tanker_only", test_sea_louse_non_tanker_only},
        {"gazelle_thresholds", test_gazelle_thresholds},
        {"regular_tanker_scorpion", test_regular_tanker_scorpion},
        {"regular_tp_grid_dimensions", test_regular_tp_grid_dimensions},
        {"animal_tier_names", test_animal_tier_names},
    };

    return bb::test::run("mgs2", tests);
}
