#include <cstdio>
#include <string_view>

#include "common/codename/codename.h"
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

const char* best(const GameStats& s)
{
    auto m = evaluate_mgs1(s);
    return m ? m->name : "<none>";
}

GameStats with(double hours)
{
    GameStats s{};
    s.play_time_seconds = hours * 3600.0;
    return s;
}

void test_big_boss_with_radar_off()
{
    GameStats s = with(2.9);
    s.alerts = 3;
    s.kills = 24;
    s.rations_used = 1;
    s.radar_off = true;
    CHECK(std::string_view(best(s)) == "BIG BOSS");
}

void test_fox_when_radar_on()
{
    GameStats s = with(2.9);
    s.alerts = 3;
    s.kills = 24;
    s.rations_used = 1;
    CHECK(std::string_view(best(s)) == "FOX");
}

void test_ladder_gated_on_unknown_time()
{
    GameStats s{};
    s.alerts = 3;
    s.kills = 24;
    CHECK(std::string_view(best(s)) != "FOX");
}

void test_falcon_precedes_jaws()
{
    GameStats s = with(2.4);
    s.kills = 300;
    CHECK(std::string_view(best(s)) == "Falcon");
}

void test_jaws()
{
    GameStats s = with(5);
    s.kills = 260;
    CHECK(std::string_view(best(s)) == "Jaws");
}

void test_pig()
{
    GameStats s = with(5);
    s.rations_used = 121;
    CHECK(std::string_view(best(s)) == "Pig");
}

void test_hippo()
{
    GameStats s = with(5);
    s.saves = 81;
    CHECK(std::string_view(best(s)) == "Hippopotamus");
}

void test_turtle()
{
    GameStats s = with(19);
    CHECK(std::string_view(best(s)) == "Turtle");
}

void test_chicken_unreachable_behind_pig()
{
    GameStats s = with(19);
    s.rations_used = 130;
    s.saves = 90;
    CHECK(std::string_view(best(s)) == "Pig");
}

void test_grid_leopard_low_ratio()
{
    GameStats s = with(5);
    s.alerts = 10;
    s.kills = 60;
    CHECK(std::string_view(best(s)) == "Leopard");
}

void test_grid_tarantula_low_kills_high_y()
{
    GameStats s = with(5);
    s.alerts = 10;
    s.kills = 20;
    CHECK(std::string_view(best(s)) == "Tarantula");
}

void test_grid_grizzly_mid_band()
{
    GameStats s = with(5);
    s.alerts = 31;
    s.kills = 103;
    CHECK(std::string_view(best(s)) == "Grizzly");
}

void test_grid_jackal_mid_cell()
{
    GameStats s = with(5);
    s.alerts = 35;
    s.kills = 60;
    CHECK(std::string_view(best(s)) == "Jackal");
}

void test_grid_gazelle_far_corner()
{
    GameStats s = with(5);
    s.alerts = 60;
    s.kills = 30;
    CHECK(std::string_view(best(s)) == "Gazelle");
}

struct TestEntry {
    const char* name;
    void (*fn)();
};

} // namespace

int main()
{
    constexpr TestEntry tests[] = {
        {"big_boss_with_radar_off", test_big_boss_with_radar_off},
        {"fox_when_radar_on", test_fox_when_radar_on},
        {"ladder_gated_on_unknown_time", test_ladder_gated_on_unknown_time},
        {"falcon_precedes_jaws", test_falcon_precedes_jaws},
        {"jaws", test_jaws},
        {"pig", test_pig},
        {"hippo", test_hippo},
        {"turtle", test_turtle},
        {"chicken_unreachable_behind_pig", test_chicken_unreachable_behind_pig},
        {"grid_leopard_low_ratio", test_grid_leopard_low_ratio},
        {"grid_tarantula_low_kills_high_y", test_grid_tarantula_low_kills_high_y},
        {"grid_grizzly_mid_band", test_grid_grizzly_mid_band},
        {"grid_jackal_mid_cell", test_grid_jackal_mid_cell},
        {"grid_gazelle_far_corner", test_grid_gazelle_far_corner},
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
