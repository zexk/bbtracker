#include <cstdio>
#include <string_view>

#include "check.h"
#include "common/codename/codename.h"
#include "common/stats.h"

using namespace bb;
using namespace bb::codename;

namespace {

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
    s.saves = 24;
    CHECK(std::string_view(best(s)) == "FOXHOUND");
    s.difficulty = Difficulty::EuroExtreme;
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

void test_cow_alerts_over_250()
{
    GameStats s = sloppy(Difficulty::VeryEasy);
    s.alerts = 251;
    s.meals_eaten = 0;
    CHECK(std::string_view(best(s)) == "Cow");
}

void test_mgs3_boundaries()
{
    GameStats s = sloppy(Difficulty::Extreme);
    s.alerts = 0;
    s.kills = 0;
    s.continues = 0;
    s.severe_injuries = 0;
    s.damage_taken_units = 0;
    s.play_time_seconds = 1;
    s.saves = 25;
    CHECK(std::string_view(best(s)) != "FOXHOUND");

    s = sloppy(Difficulty::Normal);
    s.continues = 51;
    s.kills = 150;
    s.alerts = 20;
    CHECK(std::string_view(best(s)) == "Spider");
    s.alerts = 21;
    CHECK(std::string_view(best(s)) == "Puma");

    s.kills = 50;
    s.alerts = 51;
    CHECK(std::string_view(best(s)) == "Komodo Dragon");
}

void test_exact_damage_bars_override_estimate()
{
    GameStats s = sloppy(Difficulty::Extreme);
    s.alerts = 0;
    s.kills = 0;
    s.continues = 0;
    s.severe_injuries = 0;
    s.life_med_used = 0;
    s.play_time_seconds = 1;
    s.saves = 0;
    s.damage_taken_units = 9999;
    s.damage_taken_bars = 4;
    CHECK(std::string_view(best(s)) == "FOXHOUND");
}

void test_markhor_by_capture_count()
{
    GameStats s = sloppy(Difficulty::Normal);
    s.plants_captured = 48;
    CHECK(std::string_view(best(s)) == "Markhor");
    s.plants_captured = 47;
    CHECK(std::string_view(best(s)) != "Markhor");
}

void test_kerotan_by_count()
{
    GameStats s = sloppy(Difficulty::Normal);
    s.kerotans = 64;
    CHECK(std::string_view(best(s)) == "Kerotan");
    s.kerotans = 63;
    CHECK(std::string_view(best(s)) != "Kerotan");
}

void test_leech_attached()
{
    GameStats s = sloppy(Difficulty::Normal);
    s.leech_attached = true;
    CHECK(std::string_view(best(s)) == "Leech");
    s.leech_attached = false;
    CHECK(std::string_view(best(s)) != "Leech");
}

void test_tsuchinoko_alive()
{
    GameStats s = sloppy(Difficulty::Normal);
    s.tsuchinoko_alive = true;
    CHECK(std::string_view(best(s)) == "Tsuchinoko");
    s.tsuchinoko_alive = false;
    CHECK(std::string_view(best(s)) != "Tsuchinoko");
}

void test_swallow_fast_sloppy_ve()
{
    GameStats s = sloppy(Difficulty::VeryEasy);
    s.play_time_seconds = 3600.0 * 4;
    s.severe_injuries = 100;
    s.continues = 5;
    CHECK(std::string_view(best(s)) == "Swallow");
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

void test_elite_requirements_statuses()
{
    GameStats s = sloppy(Difficulty::Extreme);
    s.kills = 0;
    s.alerts = 0;
    auto reqs = elite_requirements_mgs3(s);
    CHECK(reqs.size() == 9);
    int passing = 0;
    for (const ReqStatus& r : reqs) {
        if (r.pass) {
            ++passing;
        }
    }
    CHECK(passing < 9);

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
    CHECK(passing == 9);
}

void test_animal_tier_names()
{
    // Every shared family awards the shared table's names at each tier.
    // Base stats isolate one family: off the elite ladder (one kill),
    // off Chameleon/Pigeon (one alert, one kill), off the low-injury
    // board (twenty injuries), off the fast board (six hours).
    const Difficulty tiers[4] = {Difficulty::Extreme, Difficulty::Hard,
                                 Difficulty::Normal, Difficulty::VeryEasy};
    struct Family {
        const char* names[4]; // Extreme, Hard, Normal, VeryEasy
        GameStats stats;
    };
    auto calm = [] {
        GameStats s{};
        s.alerts = 1;
        s.kills = 1;
        s.severe_injuries = 20;
        s.play_time_seconds = 3600.0 * 6;
        return s;
    };
    GameStats low_injury = calm();
    low_injury.severe_injuries = 0;
    GameStats fast = calm();
    fast.play_time_seconds = 3600.0;
    GameStats meals = calm();
    meals.meals_eaten = 251;
    GameStats kills = calm();
    kills.kills = 251;
    GameStats time = calm();
    time.play_time_seconds = 3600.0 * 51;
    GameStats saves = calm();
    saves.saves = 101;
    GameStats worst{};
    worst.alerts = 251;
    worst.kills = 251;
    worst.play_time_seconds = 3600.0 * 51;
    worst.continues = 61;
    worst.saves = 101;
    worst.damage_taken_bars = 31;
    worst.severe_injuries = 251;
    worst.life_med_used = 11;
    const Family families[] = {
        {{"Ostrich", "Rabbit", "Mouse", "Chicken"}, worst},
        {{"Night Owl", "Flying Fox", "Bat", "Flying Squirrel"}, low_injury},
        {{"Eagle", "Hawk", "Falcon", "Swallow"}, fast},
        {{"Orca", "Jaws", "Shark", "Piranha"}, kills},
        {{"Whale", "Mammoth", "Elephant", "Pig"}, meals},
        {{"Giant Panda", "Sloth", "Capybara", "Koala"}, time},
        {{"Hippopotamus", "Zebra", "Deer", "Cat"}, saves},
    };
    for (const Family& f : families) {
        for (int i = 0; i < 4; ++i) {
            GameStats s = f.stats;
            s.difficulty = tiers[i];
            CHECK(std::string_view(best(s)) == f.names[i]);
        }
    }
}

} // namespace

int main()
{
    constexpr bb::test::Case tests[] = {
        {"foxhound_perfect_extreme", test_foxhound_perfect_extreme},
        {"fox_hard_strict", test_fox_hard_strict},
        {"fox_extreme_loose", test_fox_extreme_loose},
        {"doberman_normal_strict", test_doberman_normal_strict},
        {"hound_easy_strict", test_hound_easy_strict},
        {"chameleon_precedence_over_pigeon", test_chameleon_precedence_over_pigeon},
        {"pigeon_zero_kills_with_alerts", test_pigeon_zero_kills_with_alerts},
        {"chicken_worst_very_easy", test_chicken_worst_very_easy},
        {"cow_alerts_over_250", test_cow_alerts_over_250},
        {"mgs3_boundaries", test_mgs3_boundaries},
        {"exact_damage_bars_override_estimate", test_exact_damage_bars_override_estimate},
        {"markhor_by_capture_count", test_markhor_by_capture_count},
        {"kerotan_by_count", test_kerotan_by_count},
        {"leech_attached", test_leech_attached},
        {"tsuchinoko_alive", test_tsuchinoko_alive},
        {"swallow_fast_sloppy_ve", test_swallow_fast_sloppy_ve},
        {"regular_fallback", test_regular_fallback},
        {"elite_requirements_statuses", test_elite_requirements_statuses},
        {"animal_tier_names", test_animal_tier_names},
    };

    return bb::test::run("mgs3", tests);
}
