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
    CHECK(!evaluate_mg1(stats));
    stats.kills = 0;
    stats.play_time_seconds = 3000;
    CHECK(!evaluate_mg1(stats));
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
    CHECK(!evaluate_mg2(stats));
}

} // namespace

int main()
{
    constexpr bb::test::Case tests[] = {
        {"mg1_big_boss_time_and_kill_gates", test_mg1_big_boss_time_and_kill_gates},
        {"mg2_fox_time_gate", test_mg2_fox_time_gate},
    };

    return bb::test::run("mg12", tests);
}
