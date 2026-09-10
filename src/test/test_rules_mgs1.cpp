#include <cstdio>
#include <string_view>

#include "check.h"
#include "common/codename/codename.h"
#include "common/stats.h"

using namespace bb;
using namespace bb::codename;

namespace {

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
    s.difficulty = Difficulty::Extreme;
    s.alerts = 3;
    s.kills = 24;
    s.rations_used = 1;
    s.radar_off = true;
    CHECK(std::string_view(best(s)) == "BIG BOSS");
}

void test_fox_when_radar_on()
{
    GameStats s = with(2.9);
    s.difficulty = Difficulty::Hard;
    s.alerts = 3;
    s.kills = 24;
    s.rations_used = 1;
    CHECK(std::string_view(best(s)) == "FOX");
}

void test_elite_wrong_difficulty_rejected()
{
    GameStats s = with(2.9);
    s.difficulty = Difficulty::Easy;
    CHECK(std::string_view(best(s)) != "FOX");
}

void test_japanese_original_bypasses_difficulty()
{
    GameStats s = with(2.9);
    s.difficulty = Difficulty::Easy;
    s.mgs1_japanese_original = true;
    CHECK(std::string_view(best(s)) == "FOX");
    s.radar_off = true;
    CHECK(std::string_view(best(s)) == "BIG BOSS");
}

void test_gated_editions_reject_easy_elite()
{
    // US/PAL originals keep both gates even though their Easy tier matches
    // the JP fixed difficulty; a misdetection must not award elite ranks.
    GameStats s = with(2.9);
    s.difficulty = Difficulty::Easy;
    CHECK(std::string_view(best(s)) != "FOX");
    s.radar_off = true;
    CHECK(std::string_view(best(s)) != "BIG BOSS");
}

void test_integral_elite_ladder_uses_difficulty()
{
    GameStats s = with(2.9);
    s.mgs1_integral = true;
    s.alerts = 3;
    s.kills = 24;
    s.rations_used = 1;

    s.difficulty = Difficulty::Extreme;
    CHECK(std::string_view(best(s)) == "BIG BOSS");
    s.difficulty = Difficulty::Normal;
    CHECK(std::string_view(best(s)) == "DOBERMAN");
    s.difficulty = Difficulty::Easy;
    CHECK(std::string_view(best(s)) == "HOUND");
}

void test_integral_special_family_uses_difficulty()
{
    GameStats s = with(5);
    s.mgs1_integral = true;
    s.alerts = 10;
    s.kills = 5;

    s.difficulty = Difficulty::Extreme;
    CHECK(std::string_view(best(s)) == "Scorpion");
    s.difficulty = Difficulty::Hard;
    CHECK(std::string_view(best(s)) == "Centipede");
}

void test_integral_requirements_do_not_include_radar()
{
    GameStats s = with(2.9);
    s.mgs1_integral = true;
    const auto reqs = elite_requirements_mgs1(s);
    CHECK(reqs.size() == 5);
    for (const ReqStatus& req : reqs) {
        CHECK(std::string_view(req.label) != "radar");
    }
}

void test_ladder_gated_on_unknown_time()
{
    GameStats s{};
    s.alerts = 3;
    s.kills = 24;
    CHECK(std::string_view(best(s)) != "FOX");
}

void test_rank_cases()
{
    struct Case {
        double hours;
        int alerts;
        int kills;
        int rations;
        int saves;
        const char* expected;
    };
    constexpr Case cases[] = {
        {2.4, 0, 300, 0, 0, "Falcon"},
        {5, 0, 260, 0, 0, "Jaws"},
        {5, 0, 0, 121, 0, "Pig"},
        {5, 0, 0, 0, 81, "Hippopotamus"},
        {19, 0, 0, 0, 0, "Turtle"},
        {5, 10, 60, 0, 0, "Leopard"},
        {5, 10, 20, 0, 0, "Tarantula"},
        {5, 31, 103, 0, 0, "Grizzly"},
        {5, 35, 60, 0, 0, "Jackal"},
        {5, 60, 30, 0, 0, "Gazelle"},
    };
    for (const Case& c : cases) {
        GameStats s = with(c.hours);
        s.alerts = c.alerts;
        s.kills = c.kills;
        s.rations_used = c.rations;
        s.saves = c.saves;
        CHECK(std::string_view(best(s)) == c.expected);
    }
}

void test_chicken_unreachable_behind_pig()
{
    GameStats s = with(19);
    s.rations_used = 130;
    s.saves = 90;
    CHECK(std::string_view(best(s)) == "Pig");
}

} // namespace

int main()
{
    constexpr bb::test::Case tests[] = {
        {"big_boss_with_radar_off", test_big_boss_with_radar_off},
        {"fox_when_radar_on", test_fox_when_radar_on},
        {"elite_wrong_difficulty_rejected", test_elite_wrong_difficulty_rejected},
        {"japanese_original_bypasses_difficulty", test_japanese_original_bypasses_difficulty},
        {"gated_editions_reject_easy_elite", test_gated_editions_reject_easy_elite},
        {"integral_elite_ladder_uses_difficulty", test_integral_elite_ladder_uses_difficulty},
        {"integral_special_family_uses_difficulty", test_integral_special_family_uses_difficulty},
        {"integral_requirements_do_not_include_radar",
         test_integral_requirements_do_not_include_radar},
        {"ladder_gated_on_unknown_time", test_ladder_gated_on_unknown_time},
        {"rank_cases", test_rank_cases},
        {"chicken_unreachable_behind_pig", test_chicken_unreachable_behind_pig},
    };

    return bb::test::run("mgs1", tests);
}
