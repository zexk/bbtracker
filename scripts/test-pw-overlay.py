#!/usr/bin/env python3
"""Headless PW and MGS4 panel smoke check. Run with IMGUI_DIR and a native CXX set."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
imgui = Path(os.environ["IMGUI_DIR"])
source = (root / "src/overlay/overlay.cpp").read_text()

# Compile the actual portable draw functions without the Windows hook/backend.
def block(start, end):
    return source[source.index(start):source.index(end)]


code = r'''
#include <imgui.h>
#include <imgui_internal.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <regex>
#include <string>
#include "common/codename/codename.h"
#include "common/codename/rules_mgs4.h"
#include "overlay/overlay.h"
using namespace bb;
Game g_game = Game::MGS4;
bool (*g_clock_fn)(uint32_t&) = nullptr;
const char* mgspw_area_name(const char*, int) { return "Puerto del Alba"; }
'''
code += block("struct IdColors", "void checklist")
code += block("void draw_mgs4_feats", "constexpr const char* kMgs3Captures")
code += block("void draw_mgspw_run", "void draw_panel")
code += r'''
std::string draw(const GameStats& stats, int tab, int scroll = 0) {
    // Mimic draw_panel's 10Hz tick: the panels read matches from the cache.
    g_game = tab == 4 ? Game::MGS4 : Game::MGSPW;
    g_eval.reqs = codename::elite_requirements_mgspw(stats);
    g_eval.matches = codename::all_matches_mgs4(stats);
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(360, 480));
    ImGui::Begin("PW");
    ImGui::LogToBuffer();
    switch (tab) {
    case 0: draw_mgspw_summary(stats); break;
    case 1: draw_mgspw_global(stats, scroll); break;
    case 2: draw_mgspw_insignia(stats); break;
    case 3: draw_mgspw_codenames(stats); break;
    case 4: draw_mgs4_feats(stats, scroll); break;
    }
    std::string text = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
    return text;
}
int main() {
    char number[32];
    format_count(1234567, number, sizeof(number));
    assert(std::string(number) == "1,234,567");
    format_count(-1234, number, sizeof(number));
    assert(std::string(number) == "-1,234");
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280, 800);
    io.DeltaTime = 1.0f / 60;
    unsigned char* pixels; int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    GameStats stats;
    stats.pw_in_mission = true;
    for (int tab = 0; tab < 4; ++tab) draw(stats, tab);
    auto summary = draw(stats, 0);
    // Unknown HP is not zero health. Scoped to the HP row: the requirements
    // table legitimately shows 0% for an empty profile.
    assert(!std::regex_search(summary, std::regex(R"(HP[\s|{}]*0%)")));
    assert(std::regex_search(summary, std::regex(R"(HP[\s|{}]*-)")));
    assert(std::regex_search(summary, std::regex(R"(alerts[\s|{}]*-)")));
    assert(std::regex_search(summary, std::regex(R"(hold-ups[\s|{}]*-)")));
    assert(std::regex_search(summary, std::regex(R"(CQC uses[\s|{}]*-)")));
    assert(std::regex_search(summary, std::regex(R"(stun rod KOs[\s|{}]*-)")));
    assert(summary.find("+0 (area)") != std::string::npos);
    stats.pw_m_holdups = 3;
    stats.pw_m_cqc_uses = 2;
    stats.pw_m_stun_rod_takedowns = 1;
    stats.pw_m_heroism = 22;
    stats.pw_mission_id = 7;
    stats.pw_cur_rank = 0;
    stats.pw_cur_best = 18345;
    stats.pw_player_hp = 4500;
    stats.pw_player_max_hp = 9000;
    summary = draw(stats, 0);
    assert(summary.find("Best rank S") != std::string::npos);
    assert(summary.find("Best time 1:01.150") != std::string::npos);
    assert(summary.find("50%") != std::string::npos);
    assert(std::regex_search(summary, std::regex(R"(hold-ups[\s|{}]*3)")));
    assert(std::regex_search(summary, std::regex(R"(CQC uses[\s|{}]*2)")));
    assert(std::regex_search(summary, std::regex(R"(stun rod KOs[\s|{}]*1)")));
    assert(std::regex_search(summary, std::regex(R"(heroism[\s|{}]*\+22)")));
    // Results keeps completed sortie visible and frozen.
    GameStats idle = stats;
    std::strcpy(idle.pw_stage, "result");
    idle.pw_result_time = 15930;
    const auto results = draw(idle, 0);
    assert(results.find("0:53.100") != std::string::npos);
    assert(results.find("Best rank S") != std::string::npos);
    assert(std::regex_search(results, std::regex(R"(stun rod KOs[\s|{}]*1)")));
    assert(std::regex_search(results, std::regex(R"(hold-ups[\s|{}]*3)")));
    // Leaving results flushes run display even if game memory retains values.
    idle.pw_in_mission = false;
    std::strcpy(idle.pw_stage, "my_outer");
    const auto menu = draw(idle, 0);
    assert(menu.find("No mission running") != std::string::npos);
    assert(menu.find("headshots") == std::string::npos);
    assert(menu.find("stun rod KOs") == std::string::npos);
    assert(menu.find("Projected codename:") != std::string::npos);
    assert(menu.find("FOX / FOXHOUND") == std::string::npos);
    assert(draw(stats, 3).find("FOX / FOXHOUND") != std::string::npos);
    stats.pw_insignias = 110;
    stats.pw_headshots = 1000000;
    assert(draw(stats, 2).find("110 / 110 insignias earned") != std::string::npos);
    stats.pw_codename_axes_ok = true;
    for (int slot = 0; slot < 12; ++slot) stats.pw_codename_axes[1][slot] = 10;
    auto codenames = draw(stats, 3);
    assert(codenames.find("120") != std::string::npos);
    assert(codenames.find("100%") != std::string::npos);
    assert(codenames.find("109%") == std::string::npos); // Ungrouped slot belongs in lethality denominator.
    assert(codenames.find("No grade yet") != std::string::npos);
    assert(codenames.find("grade 1 needs") != std::string::npos);
    stats.pw_codename_result_ok = true;
    codenames = draw(stats, 3);
    assert(codenames.find("Grade 0 / 5") != std::string::npos);
    assert(codenames.find("co-op ratio") != std::string::npos);
    for (int scroll : {0, 1, 1}) draw(stats, 1, scroll);
    bool scrolled = false;
    for (auto* window : ImGui::GetCurrentContext()->Windows)
        if (std::strstr(window->Name, "pw_career_scroll")) scrolled |= window->Scroll.y > 0;
    assert(scrolled);
    draw(stats, 1, -1);
    // Mixed sleep/stun explosive takedowns, without needing those weapons in game.
    stats = GameStats{};
    stats.pw_codename_axes_ok = true;
    stats.pw_codename_axes[0][9] = 5;
    stats.pw_codename_axes[1][9] = 2;
    stats.pw_codename_axes[2][9] = 3;
    stats.pw_codename_axes[3][9] = 7;
    stats.pw_codename_axes[2][8] = 11;
    stats.pw_codename_axes[3][8] = 13;
    stats.pw_codename_axes[2][11] = 17;
    stats.pw_codename_axes[3][11] = 19;
    stats.pw_codename_axes[3][1] = 23;
    const auto career = draw(stats, 1);
    assert(career.find("grenades") < career.find("play time"));
    for (const char* pattern : {R"(grenades[\s|{}]*5[\s|{}]*12)",
                                R"(rockets[\s|{}]*0[\s|{}]*24)",
                                R"(placed explosives[\s|{}]*0[\s|{}]*36)",
                                R"(stun rod[\s|{}]*0[\s|{}]*23)",
                                R"(non-lethal takedowns[\s|{}]*95)"})
        assert(std::regex_search(career, std::regex(pattern)));
    stats.pw_codename_axes_ok = false;
    stats.pw_stun_rod_takedowns = 2;
    const auto fallback = draw(stats, 1);
    assert(std::regex_search(fallback, std::regex(R"(stun rod[\s|{}]*-[\s|{}]*2)")));
    assert(std::regex_search(fallback, std::regex(R"(grenades[\s|{}]*-[\s|{}]*-)")));
    stats.alerts = 150;
    stats.kills = 500;
    stats.continues = 50;
    stats.rations_used = 50;
    stats.play_time_seconds = 35 * 3600;
    const auto feats = draw(stats, 4);
    const auto chicken = feats.find("CHICKEN");
    assert(chicken != std::string::npos);
    assert(feats.find("GIANT PANDA") < chicken);
    const auto progress = feats.substr(chicken);
    for (const char* value : {"alerts 150 / 150", "kills 500 / 500",
                              "continues 50 / 50", "recovery 50 / 50",
                              "35:00:00 / 35:00:00"})
        assert(progress.find(value) != std::string::npos);
    ImGui::DestroyContext();
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / "test.cpp").write_text(code)
    subprocess.run(shlex.split(os.environ.get("CXX", "c++")) + [
        # -UNDEBUG: every check below is an assert, so a build type that defines
        # NDEBUG would compile the whole smoke check away and still pass.
        "-std=c++20", "-UNDEBUG", f"-I{imgui}", f"-I{root / 'src'}", str(path / "test.cpp"),
        *map(str, (root / "src/common/codename").glob("*.cpp")),
        *[str(imgui / name) for name in
          ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")],
        "-o", str(path / "test"),
    ], check=True)
    subprocess.run([str(path / "test")], check=True)
print("PW and MGS4 overlay smoke check passed")
