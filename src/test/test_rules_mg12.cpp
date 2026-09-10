#include <string_view>

#include "check.h"
#include "common/codename/codename.h"

using namespace bb;
using namespace bb::codename;

namespace {

void test_mg1_big_boss_time_and_kill_gates()
{
    GameStats stats{};
    stats.difficulty = Difficulty::Extreme;
    stats.play_time_seconds = 2999;
    auto rank = evaluate_mg1(stats);
    CHECK(rank && std::string_view(rank->name) == "BIG BOSS");
    stats.kills = 1;
    rank = evaluate_mg1(stats);
    CHECK(rank && std::string_view(rank->name) == "EAGLE");
    stats.kills = 0;
    stats.play_time_seconds = 3000;
    rank = evaluate_mg1(stats);
    CHECK(rank && std::string_view(rank->name) == "PANTHER");
}

void test_mg1_lower_rank_boundaries()
{
    GameStats stats{};
    stats.difficulty = Difficulty::Easy;
    stats.continues = 1;

    constexpr struct {
        double seconds;
        const char* name;
    } cases[] = {
        {2999, "EAGLE"}, {3000, "PANTHER"}, {5400, "JACKAL"},
        {7200, "ZEBRA"}, {14400, "DEER"}, {28800, "ELEPHANT"},
        {43200, "HIPPOPOTAMUS"}, {57600, "TURTLE"}, {72000, "CHICKEN"},
    };
    for (const auto& test : cases) {
        stats.play_time_seconds = test.seconds;
        const auto rank = evaluate_mg1(stats);
        CHECK(rank && std::string_view(rank->name) == test.name);
    }

    stats.play_time_seconds = 1;
    stats.kills = 4;
    const auto rank = evaluate_mg1(stats);
    CHECK(rank && std::string_view(rank->name) == "DEER");
}

void test_mg1_easy_alert_allowance()
{
    GameStats stats{};
    stats.difficulty = Difficulty::Easy;
    stats.play_time_seconds = 1;
    stats.alerts = 9;
    const auto easy = evaluate_mg1(stats);
    CHECK(easy && std::string_view(easy->name) == "FOX");

    stats.difficulty = Difficulty::Extreme;
    const auto original = evaluate_mg1(stats);
    CHECK(original && std::string_view(original->name) == "EAGLE");
}

void test_mg2_fox_time_gate()
{
    GameStats stats{};
    stats.difficulty = Difficulty::Easy;
    stats.play_time_seconds = 6299;
    stats.alerts = 6;
    stats.kills = 5;
    auto rank = evaluate_mg2(stats);
    CHECK(rank && std::string_view(rank->name) == "FOX");
    stats.play_time_seconds = 6300;
    rank = evaluate_mg2(stats);
    CHECK(rank && std::string_view(rank->name) == "PANTHER");
}

void test_mg2_lower_rank_boundaries()
{
    GameStats stats{};
    stats.difficulty = Difficulty::Easy;
    stats.continues = 1;

    constexpr struct {
        double seconds;
        const char* name;
    } cases[] = {
        {6299, "EAGLE"}, {6300, "PANTHER"}, {9000, "JACKAL"},
        {14400, "ZEBRA"}, {28800, "DEER"}, {43200, "ELEPHANT"},
        {54000, "HIPPOPOTAMUS"}, {64800, "TURTLE"}, {86400, "CHICKEN"},
    };
    for (const auto& test : cases) {
        stats.play_time_seconds = test.seconds;
        const auto rank = evaluate_mg2(stats);
        CHECK(rank && std::string_view(rank->name) == test.name);
    }

    stats.play_time_seconds = 1;
    stats.kills = 11;
    const auto rank = evaluate_mg2(stats);
    CHECK(rank && std::string_view(rank->name) == "ZEBRA");
}

void test_babel_rank_selection()
{
    GameStats stats{};
    stats.difficulty_raw = 3;
    stats.play_time_seconds = 7200;
    stats.alerts = 5;
    stats.kills = 24;
    stats.rations_used = 1;
    auto rank = evaluate_babel(stats);
    CHECK(rank && std::string_view(rank->name) == "BIG BOSS");

    stats.play_time_seconds = 5400;
    stats.alerts = 6;
    stats.kills = 0;
    stats.rations_used = 0;
    rank = evaluate_babel(stats);
    CHECK(rank && std::string_view(rank->name) == "FALCON");

    stats.play_time_seconds = 7201;
    stats.alerts = 0;
    stats.kills = 120;
    rank = evaluate_babel(stats);
    CHECK(rank && std::string_view(rank->name) == "JAGUAR");

    stats.kills = 0;
    stats.alerts = 26;
    rank = evaluate_babel(stats);
    CHECK(rank && std::string_view(rank->name) == "CLOW");
}

} // namespace

int main()
{
    constexpr bb::test::Case tests[] = {
        {"mg1_big_boss_time_and_kill_gates", test_mg1_big_boss_time_and_kill_gates},
        {"mg1_lower_rank_boundaries", test_mg1_lower_rank_boundaries},
        {"mg1_easy_alert_allowance", test_mg1_easy_alert_allowance},
        {"mg2_fox_time_gate", test_mg2_fox_time_gate},
        {"mg2_lower_rank_boundaries", test_mg2_lower_rank_boundaries},
        {"babel_rank_selection", test_babel_rank_selection},
    };

    return bb::test::run("mg12", tests);
}
