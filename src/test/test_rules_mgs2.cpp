#include <cstdio>
#include <string_view>

#include "common/codename/codename.h"
#include "common/codename/rules_mgs2.h"
#include "common/stats.h"

using namespace bb;
using namespace bb::codename;

namespace {

int g_fails = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++g_fails;                                                  \
        }                                                               \
    } while (0)

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
    CHECK(std::string_view(requirements.front().label) == "story (Tanker + Plant)");
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

struct TestEntry {
    const char* name;
    void (*fn)();
};

} // namespace

int main()
{
    constexpr TestEntry tests[] = {
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
    };

    for (const TestEntry& t : tests) {
        const int before = g_fails;
        t.fn();
        std::printf("[%s] %s\n", g_fails == before ? "ok" : "FAIL", t.name);
    }

    if (g_fails > 0) {
        std::printf("%d check(s) failed\n", g_fails);
        return 1;
    }
    std::printf("all %zu tests passed\n", sizeof(tests) / sizeof(tests[0]));
    return 0;
}
