#include <cstdio>
#include <string_view>

#include "common/codename/codename.h"
#include "common/codename/rules_mgs3.h"
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

GameStats sloppy(Difficulty d)
{
    GameStats s{};
    s.difficulty = d;
    s.kills = 12;
    s.alerts = 7;
    s.continues = 3;
    s.saves = 40;
    s.damage_taken_units = 600;  // ~12.5 bars
    s.play_time_seconds = 3600.0 * 9;
    s.severe_injuries = 30;
    return s;
}

const char* best(const GameStats& s)
{
    auto m = evaluate_mgs3(s);
    return m ? m->name : "<none>";
}

void test_foxhound_perfect_extreme()
{
    GameStats s = sloppy(Difficulty::Extreme);
    s.alerts = 0;
    s.kills = 0;
    s.continues = 0;
    s.severe_injuries = 19;
    s.damage_taken_units = 235;  // ~4.9 bars
    s.life_med_used = 0;
    s.play_time_seconds = 3600.0 * 4.99;
    s.saves = 25;
    CHECK(std::string_view(best(s)) == "FOXHOUND");

    auto all = all_matches_mgs3(s);
    bool has_chameleon = false;
    bool has_pigeon = false;
    for (const Match& m : all) {
        if (std::string_view(m.name) == "Chameleon") has_chameleon = true;
        if (std::string_view(m.name) == "Pigeon") has_pigeon = true;
    }
    CHECK(has_chameleon);
    CHECK(has_pigeon);
}

void test_fox_hard_strict()
{
    GameStats s = sloppy(Difficulty::Hard);
    s.alerts = 0;
    s.kills = 0;
    s.continues = 0;
    s.severe_injuries = 10;
    s.damage_taken_units = 96;   // ~2 bars
    s.play_time_seconds = 3600.0 * 4.5;
    s.saves = 24;
    CHECK(std::string_view(best(s)) == "FOX");
}

void test_fox_extreme_loose()
{
    GameStats s = sloppy(Difficulty::Extreme);
    s.alerts = 3;
    s.kills = 0;
    s.continues = 0;
    s.play_time_seconds = 3600.0 * 4.8;
    s.saves = 34;
    CHECK(std::string_view(best(s)) == "FOX");
}

void test_doberman_normal_strict()
{
    GameStats s = sloppy(Difficulty::Normal);
    s.alerts = 0;
    s.kills = 0;
    s.continues = 0;
    s.severe_injuries = 5;
    s.damage_taken_units = 48;   // ~1 bar
    s.play_time_seconds = 3600.0 * 4;
    s.saves = 20;
    CHECK(std::string_view(best(s)) == "DOBERMAN");
}

void test_hound_easy_strict()
{
    GameStats s = sloppy(Difficulty::Easy);
    s.alerts = 0;
    s.kills = 0;
    s.continues = 0;
    s.severe_injuries = 0;
    s.damage_taken_units = 24;   // ~0.5 bars
    s.play_time_seconds = 3600.0 * 3;
    s.saves = 10;
    CHECK(std::string_view(best(s)) == "HOUND");
}

void test_chameleon_precedence_over_pigeon()
{
    GameStats s = sloppy(Difficulty::Normal);
    s.kills = 0;
    s.alerts = 0;
    CHECK(std::string_view(best(s)) == "Chameleon");
}

void test_pigeon_zero_kills_with_alerts()
{
    GameStats s = sloppy(Difficulty::Hard);
    s.kills = 0;
    CHECK(std::string_view(best(s)) == "Pigeon");
}

void test_chicken_worst_very_easy()
{
    GameStats s = sloppy(Difficulty::VeryEasy);
    s.alerts = 300;
    s.kills = 300;
    s.continues = 70;
    s.saves = 120;
    s.damage_taken_units = 1680; // ~35 bars
    s.severe_injuries = 300;
    s.life_med_used = 20;
    s.play_time_seconds = 3600.0 * 60;
    CHECK(std::string_view(best(s)) == "Chicken");
}

void test_cow_alerts_over_300()
{
    GameStats s = sloppy(Difficulty::VeryEasy);
    s.alerts = 301;
    s.meals_eaten = 0;
    CHECK(std::string_view(best(s)) == "Cow");
}

void test_markhor_by_capture_count()
{
    GameStats s = sloppy(Difficulty::Normal);
    s.plants_captured = 48;
    CHECK(std::string_view(best(s)) == "Markhor");
    s.plants_captured = 47;
    CHECK(std::string_view(best(s)) != "Markhor");
}

void test_swallow_fast_sloppy_ve()
{
    GameStats s = sloppy(Difficulty::VeryEasy);
    s.play_time_seconds = 3600.0 * 4;
    s.severe_injuries = 100;
    s.continues = 5;
    CHECK(std::string_view(best(s)) == "Swallow");
}

void test_kerotan_flag_wins_special_block()
{
    GameStats s = sloppy(Difficulty::Normal);
    s.kerotan_all_shot = true;
    CHECK(std::string_view(best(s)) == "Kerotan");
}

void test_regular_fallback()
{
    GameStats s = sloppy(Difficulty::Normal);
    s.alerts = 10;
    CHECK(std::string_view(best(s)) == "Scorpion");
    s.alerts = 30;
    CHECK(std::string_view(best(s)) == "Jaguar");
    s.alerts = 80;
    CHECK(std::string_view(best(s)) == "Iguana");

    GameStats t = sloppy(Difficulty::Extreme);
    t.kills = 150;
    t.continues = 60;
    t.alerts = 10;
    CHECK(std::string_view(best(t)) == "Spider");
}

void test_tier_gating()
{
    GameStats s = sloppy(Difficulty::Extreme);
    const RankRule* fh = find_mgs3("FOXHOUND");
    CHECK(fh != nullptr);
    if (fh) {
        CHECK(!rule_matches(s, *fh));
    }
    const RankRule* fox_x = nullptr;
    for (const RankRule& r : mgs3_rules()) {
        if (r.tiers == kX && std::string_view(r.name) == "FOX") {
            fox_x = &r;
        }
    }
    CHECK(fox_x != nullptr);
}

void test_elite_requirements_statuses()
{
    GameStats s = sloppy(Difficulty::Extreme);
    s.kills = 0;
    s.alerts = 0;
    auto reqs = elite_requirements_mgs3(s);
    CHECK(reqs.size() == 8);
    int passing = 0;
    for (const ReqStatus& r : reqs) {
        if (r.pass) {
            ++passing;
        }
    }
    CHECK(passing < 8);

    s.continues = 0;
    s.severe_injuries = 0;
    s.damage_taken_units = 0;
    s.life_med_used = 0;
    s.play_time_seconds = 100;
    s.saves = 0;
    reqs = elite_requirements_mgs3(s);
    passing = 0;
    for (const ReqStatus& r : reqs) {
        if (r.pass) {
            ++passing;
        }
    }
    CHECK(passing == 8);
}

struct TestEntry {
    const char* name;
    void (*fn)();
};

} // namespace

int main()
{
    constexpr TestEntry tests[] = {
        {"foxhound_perfect_extreme", test_foxhound_perfect_extreme},
        {"fox_hard_strict", test_fox_hard_strict},
        {"fox_extreme_loose", test_fox_extreme_loose},
        {"doberman_normal_strict", test_doberman_normal_strict},
        {"hound_easy_strict", test_hound_easy_strict},
        {"chameleon_precedence_over_pigeon", test_chameleon_precedence_over_pigeon},
        {"pigeon_zero_kills_with_alerts", test_pigeon_zero_kills_with_alerts},
        {"chicken_worst_very_easy", test_chicken_worst_very_easy},
        {"cow_alerts_over_300", test_cow_alerts_over_300},
        {"markhor_by_capture_count", test_markhor_by_capture_count},
        {"swallow_fast_sloppy_ve", test_swallow_fast_sloppy_ve},
        {"kerotan_flag_wins_special_block", test_kerotan_flag_wins_special_block},
        {"regular_fallback", test_regular_fallback},
        {"tier_gating", test_tier_gating},
        {"elite_requirements_statuses", test_elite_requirements_statuses},
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
