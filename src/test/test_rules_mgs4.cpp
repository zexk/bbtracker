#include <cstdio>
#include <string_view>

#include "common/codename/codename.h"
#include "common/codename/rules_mgs4.h"

using namespace bb;
using namespace bb::codename;

namespace {

int fails = 0;
#define CHECK(condition)                                                  \
    do {                                                                  \
        if (!(condition)) {                                               \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++fails;                                                      \
        }                                                                 \
    } while (0)

const char* best(const GameStats& stats)
{
    const auto match = evaluate_mgs4(stats);
    return match ? match->name : "<none>";
}

bool has(const GameStats& stats, std::string_view name)
{
    for (const Match& match : all_matches_mgs4(stats)) {
        if (match.name == name) return true;
    }
    return false;
}

void test_elite_ladder()
{
    GameStats stats{};
    stats.difficulty = Difficulty::Extreme;
    stats.play_time_seconds = 4 * 3600;
    CHECK(std::string_view(best(stats)) == "BIG BOSS");

    stats.alerts = 1;
    CHECK(std::string_view(best(stats)) == "FOX HOUND");
    stats.difficulty = Difficulty::Normal;
    CHECK(std::string_view(best(stats)) == "FOX");
    stats.difficulty = Difficulty::Easy;
    CHECK(std::string_view(best(stats)) == "HOUND");
    stats.difficulty = Difficulty::VeryEasy;
    CHECK(std::string_view(best(stats)) == "WOLF");
}

void test_elite_strict_boundaries()
{
    GameStats stats{};
    stats.difficulty = Difficulty::Extreme;
    stats.play_time_seconds = 5 * 3600;
    CHECK(std::string_view(best(stats)) == "FOX HOUND");

    stats.play_time_seconds = 5 * 3600 - 1;
    stats.alerts = 3;
    CHECK(std::string_view(best(stats)) == "FOX");
}

void test_specials_and_multiple_matches()
{
    GameStats stats{};
    stats.difficulty = Difficulty::Normal;
    stats.play_time_seconds = 10 * 3600;
    stats.kills = 1;
    stats.alerts = 10;
    stats.continues = 1;
    stats.headshots = 150;
    stats.hold_ups = 50;
    CHECK(has(stats, "EAGLE"));
    CHECK(has(stats, "GIBBON"));
    CHECK(std::string_view(best(stats)) == "EAGLE");

    stats.side_rolls = 100;
    stats.forward_rolls = 199;
    CHECK(has(stats, "SCARAB"));
    CHECK(!has(stats, "FROG"));
    ++stats.forward_rolls;
    CHECK(has(stats, "FROG"));
}

void test_regular_grid_boundaries()
{
    GameStats stats{};
    stats.difficulty = Difficulty::Normal;
    stats.play_time_seconds = 10 * 3600;
    stats.alerts = 74;
    stats.kills = 249;
    stats.continues = 24;
    CHECK(has(stats, "SCORPION"));

    stats.alerts = 75;
    CHECK(has(stats, "JAGUAR"));
    stats.kills = 250;
    CHECK(has(stats, "PANTHER"));
    stats.continues = 25;
    CHECK(has(stats, "PUMA"));
}

void test_chicken_priority()
{
    GameStats stats{};
    stats.difficulty = Difficulty::VeryEasy;
    stats.alerts = 150;
    stats.kills = 500;
    stats.continues = 50;
    stats.rations_used = 50;
    stats.play_time_seconds = 35 * 3600;
    CHECK(has(stats, "CHICKEN"));
    CHECK(std::string_view(best(stats)) == "PIG");
}

} // namespace

int main()
{
    test_elite_ladder();
    test_elite_strict_boundaries();
    test_specials_and_multiple_matches();
    test_regular_grid_boundaries();
    test_chicken_priority();
    std::printf("%s\n", fails ? "MGS4 rules failed" : "MGS4 rules passed");
    return fails != 0;
}
