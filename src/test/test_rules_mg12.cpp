#include <cassert>
#include <string_view>

#include "common/codename/codename.h"

int main()
{
    bb::GameStats stats{};
    stats.difficulty = bb::Difficulty::Extreme;
    stats.play_time_seconds = 2999;
    auto rank = bb::codename::evaluate_mg1(stats);
    assert(rank && std::string_view(rank->name) == "BIG BOSS");
    stats.kills = 1;
    assert(!bb::codename::evaluate_mg1(stats));
    stats.kills = 0;
    stats.play_time_seconds = 3000;
    assert(!bb::codename::evaluate_mg1(stats));

    stats = {};
    stats.difficulty = bb::Difficulty::Easy;
    stats.play_time_seconds = 6299;
    stats.alerts = 6;
    stats.kills = 5;
    rank = bb::codename::evaluate_mg2(stats);
    assert(rank && std::string_view(rank->name) == "FOX");
    stats.play_time_seconds = 6300;
    assert(!bb::codename::evaluate_mg2(stats));
}
