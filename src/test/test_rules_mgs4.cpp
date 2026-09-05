#include <cstdio>
#include <string_view>

#include "check.h"
#include "common/codename/codename.h"
#include "common/codename/rules_mgs4.h"

using namespace bb;
using namespace bb::codename;

namespace {

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
    CHECK(std::string_view(best(stats)) == "BIG BOSS");

    ++stats.play_time_seconds;
    CHECK(std::string_view(best(stats)) == "FOX HOUND");

    --stats.play_time_seconds;
    stats.alerts = 3;
    CHECK(std::string_view(best(stats)) == "FOX HOUND");
    ++stats.alerts;
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
    stats.alerts = 75;
    stats.kills = 250;
    stats.continues = 25;
    CHECK(has(stats, "SCORPION"));

    ++stats.alerts;
    CHECK(has(stats, "JAGUAR"));
    ++stats.kills;
    CHECK(has(stats, "PANTHER"));
    ++stats.continues;
    CHECK(has(stats, "PUMA"));
}

void test_other_inclusive_boundaries()
{
    GameStats stats{};
    stats.difficulty = Difficulty::Normal;
    stats.play_time_seconds = 30 * 3600;
    stats.alerts = 25;
    stats.kills = 1;
    stats.continues = 1;
    stats.knife_defeats = 50;
    stats.cqc_holds = 50;
    CHECK(has(stats, "ASSASSIN"));
    CHECK(has(stats, "GIANT PANDA"));
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
    constexpr bb::test::Case tests[] = {
        {"elite_ladder", test_elite_ladder},
        {"elite_strict_boundaries", test_elite_strict_boundaries},
        {"specials_and_multiple_matches", test_specials_and_multiple_matches},
        {"regular_grid_boundaries", test_regular_grid_boundaries},
        {"other_inclusive_boundaries", test_other_inclusive_boundaries},
        {"chicken_priority", test_chicken_priority},
    };

    return bb::test::run("mgs4", tests);
}
