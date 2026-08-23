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

GameStats with(Difficulty d, int disc, int kills, int rations, int cont, int saves,
               double hours)
{
    GameStats s{};
    s.difficulty = d;
    s.alerts = disc;
    s.kills = kills;
    s.rations_used = rations;
    s.continues = cont;
    s.saves = saves;
    if (hours > 0.0) {
        s.play_time_seconds = hours * 3600.0;
    }
    return s;
}

void test_ladder_big_boss_extreme()
{
    GameStats s = with(Difficulty::Extreme, 3, 24, 1, 0, 10, 2.9);
    CHECK(std::string_view(best(s)) == "BIG BOSS");
}

void test_ladder_fox_hard()
{
    GameStats s = with(Difficulty::Hard, 3, 24, 1, 0, 10, 2.9);
    CHECK(std::string_view(best(s)) == "FOX");
}

void test_ladder_time_unknown_gates_elite()
{
    GameStats s = with(Difficulty::Extreme, 3, 24, 1, 0, 10, 0.0);
    CHECK(std::string_view(best(s)) != "BIG BOSS");
}

void test_scorpion_family()
{
    GameStats x = with(Difficulty::Extreme, 10, 5, 0, 0, 0, 5.0);
    CHECK(std::string_view(best(x)) == "Scorpion");
    GameStats ve = with(Difficulty::VeryEasy, 10, 5, 0, 0, 0, 5.0);
    CHECK(std::string_view(best(ve)) == "Spider");
}

void test_jaguar_family()
{
    GameStats h = with(Difficulty::Hard, 10, 70, 0, 0, 0, 5.0);
    CHECK(std::string_view(best(h)) == "Panther");
}

void test_eagle_family_needs_time()
{
    GameStats n = with(Difficulty::Normal, 60, 60, 0, 0, 0, 0.0);
    CHECK(std::string_view(best(n)) != "Falcon");

    GameStats t = with(Difficulty::Normal, 60, 60, 0, 0, 0, 2.4);
    CHECK(std::string_view(best(t)) == "Falcon");
}

void test_orca_kills()
{
    GameStats x = with(Difficulty::Extreme, 60, 260, 0, 0, 0, 20.0);
    CHECK(std::string_view(best(x)) == "Orca");
}

void test_whale_rations()
{
    GameStats e = with(Difficulty::Easy, 60, 5, 131, 0, 0, 5.0);
    CHECK(std::string_view(best(e)) == "Pig");
}

void test_hippo_saves()
{
    GameStats n = with(Difficulty::Normal, 60, 5, 0, 0, 81, 5.0);
    CHECK(std::string_view(best(n)) == "Deer");
}

void test_panda_time()
{
    GameStats h = with(Difficulty::Hard, 60, 5, 0, 0, 0, 19.0);
    CHECK(std::string_view(best(h)) == "Sloth");
}

void test_night_owl_two_ranges()
{
    GameStats a = with(Difficulty::Hard, 40, 50, 0, 0, 0, 6.0);
    CHECK(std::string_view(best(a)) == "Flying Fox");
    GameStats b = with(Difficulty::Extreme, 56, 57, 0, 0, 0, 6.0);
    CHECK(std::string_view(best(b)) == "Night Owl");
}

void test_croc_two_ranges()
{
    GameStats a = with(Difficulty::Extreme, 40, 170, 0, 0, 0, 6.0);
    CHECK(std::string_view(best(a)) == "Crocodile");
    GameStats b = with(Difficulty::Normal, 56, 100, 0, 0, 0, 6.0);
    CHECK(std::string_view(best(b)) == "Iguana");
}

void test_worst_chicken()
{
    GameStats e = with(Difficulty::Easy, 200, 140, 140, 70, 90, 19.0);
    CHECK(std::string_view(best(e)) == "Pig");
}

struct TestEntry {
    const char* name;
    void (*fn)();
};

} // namespace

int main()
{
    constexpr TestEntry tests[] = {
        {"ladder_big_boss_extreme", test_ladder_big_boss_extreme},
        {"ladder_fox_hard", test_ladder_fox_hard},
        {"ladder_time_unknown_gates_elite", test_ladder_time_unknown_gates_elite},
        {"scorpion_family", test_scorpion_family},
        {"jaguar_family", test_jaguar_family},
        {"eagle_family_needs_time", test_eagle_family_needs_time},
        {"orca_kills", test_orca_kills},
        {"whale_rations", test_whale_rations},
        {"hippo_saves", test_hippo_saves},
        {"panda_time", test_panda_time},
        {"night_owl_two_ranges", test_night_owl_two_ranges},
        {"croc_two_ranges", test_croc_two_ranges},
        {"worst_chicken", test_worst_chicken},
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
