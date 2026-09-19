#include <cstdio>
#include <string_view>

#include "check.h"
#include "common/codename/codename.h"
#include "common/codename/rules_mgs4.h"
#include "games/mgs4/flashbacks.h"

using namespace bb;
using namespace bb::codename;

namespace {

void test_flashback_story_order()
{
    bool seen[222] = {};
    size_t act1 = 0, act2_briefing = 0, act2 = 0;
    for (size_t position = 0; position < std::size(mgs4::kFlashbacksStoryOrder); ++position) {
        const auto bit_index = mgs4::kFlashbacksStoryOrder[position];
        if (bit_index >= 222) {
            CHECK(bit_index < 222);
            continue;
        }
        CHECK(!seen[bit_index]);
        seen[bit_index] = true;
        if (bit_index == 3) act1 = position;
        if (bit_index == 8) act2_briefing = position;
        if (bit_index == 25) act2 = position;
    }
    CHECK(act1 < act2_briefing && act2_briefing < act2);
}

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
    // PIG sits below the GECKO line, so the results screen shows CHICKEN.
    CHECK(std::string_view(best(stats)) == "CHICKEN");

    // BEE is one place above the line and keeps the screen.
    stats.syringe_uses = 50;
    CHECK(has(stats, "BEE"));
    CHECK(std::string_view(best(stats)) == "BEE");
}

void test_little_gray_claimed()
{
    GameStats stats{};
    stats.difficulty = Difficulty::Normal;
    stats.play_time_seconds = 10 * 3600;
    stats.alerts = 10;
    stats.kills = 1;
    stats.continues = 1;
    stats.weapons_acquired = mgs4_goals::kLittleGrayWeapons;
    CHECK(has(stats, "LITTLE GRAY"));
    CHECK(std::string_view(best(stats)) == "LITTLE GRAY");

    // Second run on the same save: the emblem still matches, but the game
    // passes over it and shows the next one down.
    stats.little_gray_claimed = true;
    CHECK(has(stats, "LITTLE GRAY"));
    CHECK(std::string_view(best(stats)) == "SCORPION");

    // A better emblem outranks LITTLE GRAY either way.
    stats.little_gray_claimed = false;
    stats.praises = mgs4_goals::kHawkPraises;
    CHECK(std::string_view(best(stats)) == "HAWK");
}

void test_rule_order_matches_emblem_ids()
{
    const auto rules = mgs4_rules();
    CHECK(rules.size() == 40);
    CHECK(std::string_view(rules[kLittleGrayRule].name) == "LITTLE GRAY");
    CHECK(std::string_view(rules[kChickenRule].name) == "CHICKEN");
    CHECK(std::string_view(rules[kChickenOutranksFrom].name) == "GECKO");
}

void test_elite_requirements_mirror_big_boss()
{
    // The requirements panel reads the same ladder the rank matches on:
    // a clean run passes every row, one alert fails exactly the alerts row.
    GameStats stats{};
    stats.difficulty = Difficulty::Extreme;
    stats.play_time_seconds = 4 * 3600;
    const auto clean = elite_requirements_mgs4(stats);
    CHECK(clean.size() == 6);
    for (const ReqStatus& r : clean) {
        CHECK(r.pass);
    }
    stats.alerts = 1;
    const auto dirty = elite_requirements_mgs4(stats);
    CHECK(dirty.size() == 6);
    int fails = 0;
    for (const ReqStatus& r : dirty) {
        if (!r.pass) {
            ++fails;
            CHECK(std::string_view(r.label) == "alerts");
        }
    }
    CHECK(fails == 1);
}

} // namespace

int main()
{
    constexpr bb::test::Case tests[] = {
        {"flashback_story_order", test_flashback_story_order},
        {"elite_ladder", test_elite_ladder},
        {"elite_strict_boundaries", test_elite_strict_boundaries},
        {"specials_and_multiple_matches", test_specials_and_multiple_matches},
        {"regular_grid_boundaries", test_regular_grid_boundaries},
        {"other_inclusive_boundaries", test_other_inclusive_boundaries},
        {"chicken_priority", test_chicken_priority},
        {"little_gray_claimed", test_little_gray_claimed},
        {"rule_order_matches_emblem_ids", test_rule_order_matches_emblem_ids},
        {"elite_requirements_mirror_big_boss", test_elite_requirements_mirror_big_boss},
    };

    return bb::test::run("mgs4", tests);
}
