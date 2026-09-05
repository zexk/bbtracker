#include <cstdio>
#include <string_view>

#include "common/codename/codename.h"

using namespace bb;
using namespace bb::codename;

namespace {

int fails = 0;
#define CHECK(condition)                                                     \
    do {                                                                     \
        if (!(condition)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++fails;                                                         \
        }                                                                    \
    } while (0)

const char* title(const GameStats& stats)
{
    const auto match = evaluate_mgspw(stats);
    return match ? match->name : "<none>";
}

// Career counters only: the profile the tracker falls back to when the native
// evaluator axes have not been read.
GameStats short_class()
{
    GameStats stats{};
    stats.pw_pistol_takedowns = 10;
    stats.pw_tranq = 10;
    stats.pw_kills = 0;
    stats.pw_camaraderie = 0;
    return stats;
}

// Four classes, none holding 40% of the takedowns: the all-weapons branch.
GameStats all_weapons()
{
    GameStats stats{};
    stats.pw_pistol_takedowns = 10;
    stats.pw_ar_takedowns = 10;
    stats.pw_sniper_takedowns = 10;
    stats.pw_grenade_takedowns = 10;
    stats.pw_cqc_takedowns = 10;
    stats.pw_tranq = 10;
    stats.pw_kills = 0;
    stats.pw_camaraderie = 0;
    return stats;
}

void test_no_takedowns_has_no_title()
{
    GameStats stats{};
    CHECK(!evaluate_mgspw(stats).has_value());
    // Unresolved counters read -1 and must not classify as negative takedowns.
    stats.pw_kills = -1;
    stats.pw_tranq = -1;
    stats.pw_cqc_takedowns = -1;
    CHECK(!evaluate_mgspw(stats).has_value());
}

void test_dominant_class_and_axes()
{
    GameStats stats = short_class();
    CHECK(std::string_view(title(stats)) == "BUTTERFLY");
    // Camaraderie over 10000 is the cooperation half of the table.
    stats.pw_camaraderie = 10001;
    CHECK(std::string_view(title(stats)) == "ANT");
    // Non-lethal must beat twice lethal, so equal counts read as lethal.
    stats.pw_camaraderie = 0;
    stats.pw_kills = 5;
    CHECK(std::string_view(title(stats)) == "SCORPION");
}

void test_all_weapons_spread()
{
    GameStats stats = all_weapons();
    CHECK(std::string_view(title(stats)) == "FOX");
    CHECK(evaluate_mgspw(stats)->kind == Kind::Elite);
    stats.pw_kills = 10;
    CHECK(std::string_view(title(stats)) == "HOUND");
    stats.pw_camaraderie = 10001;
    CHECK(std::string_view(title(stats)) == "DOBERMAN");
    stats.pw_kills = 0;
    CHECK(std::string_view(title(stats)) == "FOXHOUND");

    // A class holding exactly 40% is not a spread: the gate is strict.
    GameStats top_heavy{};
    top_heavy.pw_pistol_takedowns = 40;
    top_heavy.pw_ar_takedowns = 20;
    top_heavy.pw_sniper_takedowns = 20;
    top_heavy.pw_grenade_takedowns = 20;
    top_heavy.pw_tranq = 40;
    CHECK(std::string_view(title(top_heavy)) == "BUTTERFLY");
}

void test_native_axes()
{
    GameStats stats{};
    stats.pw_codename_axes_ok = true;
    stats.pw_codename_axes[0][2] = 3;  // lethal, short
    stats.pw_codename_axes[1][6] = 4;  // sleep, short
    stats.pw_codename_axes[2][4] = 5;  // stun, long
    const PwAxes axes = pw_axes(stats);
    CHECK(axes.native);
    CHECK(axes.slot[2] == 3 && axes.slot[6] == 4 && axes.slot[4] == 5);
    CHECK(axes.by_class[0] == 7);  // short
    CHECK(axes.by_class[2] == 5);  // long
    CHECK(axes.total == 12);
    CHECK(axes.lethal == 3);
    CHECK(axes.nonlethal == 9);
    CHECK(axes.classes_used == 2);

    // Slot 10 belongs to no class, so it counts as neither a class nor total.
    GameStats ungrouped{};
    ungrouped.pw_codename_axes_ok = true;
    ungrouped.pw_codename_axes[0][10] = 5;
    const PwAxes odd = pw_axes(ungrouped);
    CHECK(odd.slot[10] == 5);
    CHECK(odd.total == 0);
    CHECK(odd.lethal == 5);
    CHECK(odd.classes_used == 0);

    // Without the axes the fallback profile fills by_class instead.
    const PwAxes fallback = pw_axes(all_weapons());
    CHECK(!fallback.native);
    CHECK(fallback.total == 50);
    CHECK(fallback.classes_used == 5);
}

void test_class_names_and_gates()
{
    CHECK(std::string_view(pw_class_name(0)) == "short");
    CHECK(std::string_view(pw_class_name(5)) == "stun rod");
    CHECK(std::string_view(pw_class_name(-1)) == "?");
    CHECK(std::string_view(pw_class_name(6)) == "?");

    CHECK(pw_grade_gate(1).camaraderie == 10000);
    CHECK(pw_grade_gate(1).heroism == 10000);
    CHECK(pw_grade_gate(3).coop_ratio == 1.0);
    CHECK(pw_grade_gate(5).camaraderie == 500000);
    CHECK(pw_grade_gate(0).camaraderie == 0);
    CHECK(pw_grade_gate(6).camaraderie == 0);
}

void test_grade_ladder()
{
    // The evaluator has not run yet, so there is no grade to report.
    GameStats stats = short_class();
    stats.pw_codename_missions_required = 10;
    stats.pw_codename_missions_counted = 10;
    CHECK(pw_grade(stats).grade == 0);

    stats.pw_codename_result_ok = true;
    // Ratio 1.0 clears grades 1 to 3; grade 4 waits on its own flag.
    PwGrade grade = pw_grade(stats);
    CHECK(grade.grade == 3);
    CHECK(grade.next == 4);
    CHECK(std::string_view(grade.blocker) == "mission ranks (grade 4 flag)");

    // Grades 1 and 2 compare the ratio strictly, so the ratio has to beat
    // grade 2's gate rather than meet it; grade 3 and up accept 1.0 exactly.
    stats.pw_codename_missions_counted = 5;
    grade = pw_grade(stats);
    CHECK(grade.grade == 1);
    CHECK(grade.next == 2);
    CHECK(std::string_view(grade.blocker) == "cooperation ratio");
    CHECK(grade.have == 0.5 && grade.need == 0.5);
    stats.pw_codename_missions_counted = 1;  // ratio 0.1, over the 0.05 gate
    CHECK(pw_grade(stats).grade == 1);

    // A solo title needs camaraderie at or below the step, a cooperation
    // title strictly above it.
    stats.pw_codename_missions_counted = 10;
    stats.pw_camaraderie = 10000;
    CHECK(pw_grade(stats).grade == 3);
    // Over the step it reads as a cooperation title, which clears grade 1 by
    // being over that same step and then waits on the next one.
    stats.pw_camaraderie = 10001;
    CHECK(pw_grade(stats).grade == 1);
    CHECK(std::string_view(pw_grade(stats).blocker) == "camaraderie over");
    stats.pw_camaraderie = 100001;
    CHECK(pw_grade(stats).grade == 3);

    // Both flags set and the top steps cleared reaches grade 5.
    stats.pw_camaraderie = 500001;
    stats.pw_codename_grade4_ok = true;
    stats.pw_codename_grade5_ok = true;
    grade = pw_grade(stats);
    CHECK(grade.grade == 5);
    CHECK(grade.next == 0);
    CHECK(grade.blocker == nullptr);
}

void test_grade_heroism_floor()
{
    // Only all-weapons titles carry a Heroism floor, and it is strict.
    GameStats stats = all_weapons();
    stats.pw_codename_result_ok = true;
    stats.pw_codename_missions_required = 10;
    stats.pw_codename_missions_counted = 1;
    stats.pw_heroism = 10000;
    PwGrade grade = pw_grade(stats);
    CHECK(grade.grade == 0);
    CHECK(std::string_view(grade.blocker) == "heroism");
    CHECK(grade.need == 10000);
    stats.pw_heroism = 10001;
    CHECK(pw_grade(stats).grade == 1);

    GameStats solo = short_class();
    solo.pw_codename_result_ok = true;
    solo.pw_codename_missions_required = 10;
    solo.pw_codename_missions_counted = 1;
    solo.pw_heroism = 0;
    CHECK(pw_grade(solo).grade == 1);
}

void test_elite_requirements()
{
    // Career counters: the spread is reported as classes and top share.
    const auto rows = elite_requirements_mgspw(all_weapons());
    CHECK(rows.size() == 3);
    CHECK(std::string_view(rows[0].label) == "classes used");
    CHECK(rows[0].pass && rows[0].current == 5 && rows[0].limit == 4);
    CHECK(std::string_view(rows[1].label) == "top class %");
    CHECK(rows[1].pass && rows[1].current == 20.0 && rows[1].limit == 40.0);
    CHECK(std::string_view(rows[2].label) == "non-lethal");
    // Non-lethal counts the tranquillizer and CQC banks together.
    CHECK(rows[2].pass && rows[2].current == 20 && rows[2].limit == 0);

    // Native axes: the evaluator measures the twelve slots against their own
    // average, so the same spread is reported as one deviation.
    GameStats balanced{};
    balanced.pw_codename_axes_ok = true;
    for (int slot = 0; slot < 12; ++slot) {
        balanced.pw_codename_axes[1][slot] = 10;
    }
    const auto native = elite_requirements_mgspw(balanced);
    CHECK(native.size() == 2);
    CHECK(std::string_view(native[0].label) == "slot spread %");
    CHECK(native[0].pass && native[0].limit == 10.0);
    CHECK(native[1].pass);  // 120 non-lethal against no kills

    balanced.pw_codename_axes[1][3] = 40;
    const auto uneven = elite_requirements_mgspw(balanced);
    CHECK(!uneven[0].pass);

    // No takedowns at all is not a passing spread.
    const auto empty = elite_requirements_mgspw(GameStats{});
    CHECK(!empty[0].pass && !empty[1].pass);
}

void test_insignias()
{
    CHECK(std::string_view(pw_insignia(1).name) == "Stealth Master (Rank C)");
    CHECK(pw_insignia(1).over == 25);
    CHECK(pw_insignia(1).heroism == 500);
    CHECK(std::string_view(pw_insignia(0).name) == "?");
    CHECK(std::string_view(pw_insignia(111).name) == "?");
    CHECK(pw_insignia(111).over == -1);

    GameStats stats{};
    stats.pw_noalert_clears = 7;
    stats.pw_headshots = 42;
    CHECK(pw_insignia_progress(1, stats) == 7);
    CHECK(pw_insignia_progress(16, stats) == 42);
    // Families whose counter is not resolved report no progress.
    CHECK(pw_insignia_progress(13, stats) == -1);
    CHECK(pw_insignia_progress(110, stats) == -1);
}

} // namespace

int main()
{
    test_no_takedowns_has_no_title();
    test_dominant_class_and_axes();
    test_all_weapons_spread();
    test_native_axes();
    test_class_names_and_gates();
    test_grade_ladder();
    test_grade_heroism_floor();
    test_elite_requirements();
    test_insignias();
    if (fails == 0) {
        std::printf("mgspw rules: all checks passed\n");
    }
    return fails == 0 ? 0 : 1;
}
