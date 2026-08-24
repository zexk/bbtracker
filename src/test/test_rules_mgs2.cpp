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
    s.shots_fired = 699;
    s.alerts = 3;
    s.damage_taken_units = 479;
    s.saves = 8;
    CHECK(std::string_view(best(s)) == "BIG BOSS");
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

void test_fox_extreme_loose()
{
    GameStats s = base(Difficulty::Extreme, 32);
    s.alerts = 3;
    s.saves = 16;
    CHECK(std::string_view(best(s)) == "FOX");
}

void test_doberman_euro_extreme_mid_row()
{
    GameStats s = base(Difficulty::EuroExtreme, 32);
    s.alerts = 3;
    s.saves = 16;
    CHECK(std::string_view(best(s)) == "DOBERMAN");

    GameStats t = base(Difficulty::EuroExtreme, 32);
    t.radar_off = true;
    t.shots_fired = 100;
    t.damage_taken_units = 100;
    t.play_time_seconds = 3600.0 * 2;
    t.saves = 8;
    CHECK(std::string_view(best(t)) == "FOX");
}

void test_hound_normal_strict_row()
{
    GameStats s = base(Difficulty::Normal, 32);
    s.radar_off = true;
    s.shots_fired = 100;
    s.damage_taken_units = 100;
    s.play_time_seconds = 3600.0 * 2;
    s.saves = 8;
    CHECK(std::string_view(best(s)) == "HOUND");
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
    s.alerts = 251;
    s.kills = 251;
    s.rations_used = 32;
    s.continues = 61;
    s.saves = 101;
    s.play_time_seconds = 3600.0 * 31;
    CHECK(std::string_view(best(s)) == "Chicken");
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
    s.alerts = 51;
    s.play_time_seconds = 3600.0 * 6;
    CHECK(std::string_view(best(s)) == "Cow");

    GameStats t = base(Difficulty::VeryEasy, 32);
    t.kills = 5;
    t.alerts = 51;
    t.play_time_seconds = 3600.0 * 6;
    CHECK(std::string_view(best(t)) != "Cow");
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
    CHECK(std::string_view(best(s)) == "Comodo Dragon");
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
        {"fox_extreme_loose", test_fox_extreme_loose},
        {"doberman_euro_extreme_mid_row", test_doberman_euro_extreme_mid_row},
        {"hound_normal_strict_row", test_hound_normal_strict_row},
        {"pigeon_zero_kills", test_pigeon_zero_kills},
        {"worst_chicken_normal", test_worst_chicken_normal},
        {"swallow_tanker_fast_ve", test_swallow_tanker_fast_ve},
        {"cow_alerts_by_mission", test_cow_alerts_by_mission},
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
