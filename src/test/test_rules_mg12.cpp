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

    bb::GameStats pw{};
    pw.pw_codename_axes_ok = true;
    pw.pw_codename_axes[0][3] = 1;
    assert(std::string_view(bb::codename::evaluate_mgspw(pw)->name) == "PUMA");

    pw = {};
    pw.pw_codename_axes_ok = true;
    pw.pw_codename_axes[1][2] = 1;
    assert(std::string_view(bb::codename::evaluate_mgspw(pw)->name) == "BUTTERFLY");
    pw.pw_camaraderie = 10001;
    assert(std::string_view(bb::codename::evaluate_mgspw(pw)->name) == "ANT");

    pw = {};
    pw.pw_codename_axes_ok = true;
    for (int slot = 0; slot < 12; ++slot) pw.pw_codename_axes[1][slot] = 10;
    assert(std::string_view(bb::codename::evaluate_mgspw(pw)->name) == "FOX");
    pw.pw_camaraderie = 10001;
    assert(std::string_view(bb::codename::evaluate_mgspw(pw)->name) == "FOXHOUND");

    // Grade ladder. Without the native result block there is no grade at all,
    // however good the profile looks.
    pw = {};
    pw.pw_codename_axes_ok = true;
    pw.pw_codename_axes[1][2] = 1;
    assert(bb::codename::pw_grade(pw).grade == 0);

    // Solo short-range non-lethal (BUTTERFLY): cooperation ratio drives the
    // first grades, and a solo title needs camaraderie at or below the step.
    pw.pw_codename_result_ok = true;
    pw.pw_codename_missions_required = 100;
    pw.pw_codename_missions_counted = 4;   // ratio 0.04, under the 0.05 gate
    auto g = bb::codename::pw_grade(pw);
    assert(g.grade == 0 && g.next == 1
           && std::string_view(g.blocker) == "cooperation ratio");
    pw.pw_codename_missions_counted = 6;   // 0.06 clears grade 1, not grade 2
    g = bb::codename::pw_grade(pw);
    assert(g.grade == 1 && g.next == 2);
    pw.pw_codename_missions_counted = 100; // ratio 1.0 reaches grade 3
    g = bb::codename::pw_grade(pw);
    assert(g.grade == 3 && g.next == 4
           && std::string_view(g.blocker).find("grade 4") != std::string_view::npos);
    pw.pw_codename_grade4_ok = true;
    pw.pw_codename_grade5_ok = true;
    assert(bb::codename::pw_grade(pw).grade == 5);

    // All-weapons titles add a Heroism floor: same profile, spread evenly, is
    // held at grade 0 until Heroism reaches 10k.
    pw = {};
    pw.pw_codename_axes_ok = true;
    pw.pw_codename_result_ok = true;
    pw.pw_codename_missions_required = 1;
    pw.pw_codename_missions_counted = 1;
    pw.pw_codename_grade4_ok = true;
    pw.pw_codename_grade5_ok = true;
    for (int slot = 0; slot < 12; ++slot) pw.pw_codename_axes[1][slot] = 10;
    g = bb::codename::pw_grade(pw);
    assert(g.grade == 0 && std::string_view(g.blocker) == "heroism"
           && g.need == 10000);
    // The floor is strictly greater, so landing exactly on it earns nothing.
    pw.pw_heroism = 10000;
    assert(bb::codename::pw_grade(pw).grade == 0);
    pw.pw_heroism = 10001;
    assert(bb::codename::pw_grade(pw).grade == 1);
    pw.pw_heroism = 250000;
    assert(bb::codename::pw_grade(pw).grade == 4);
    pw.pw_heroism = 250001;
    assert(bb::codename::pw_grade(pw).grade == 5);
}
