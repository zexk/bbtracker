#!/usr/bin/env python3
"""Headless PW panel smoke check. Run with IMGUI_DIR and a native CXX set."""
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
#include "overlay/overlay.h"
using namespace bb;
bool (*g_clock_fn)(uint32_t&) = nullptr;
const char* mgspw_area_name(const char*, int) { return "Puerto del Alba"; }
'''
code += block("struct IdColors", "void checklist")
code += block("void draw_mgspw_summary", "void draw_panel")
code += r'''
std::string draw(const GameStats& stats, int tab, int scroll = 0) {
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(420, 700));
    ImGui::Begin("PW");
    ImGui::LogToBuffer();
    switch (tab) {
    case 0: draw_mgspw_summary(stats); break;
    case 1: draw_mgspw_global(stats, scroll); break;
    case 2: draw_mgspw_insignia(stats); break;
    case 3: draw_mgspw_codenames(stats); break;
    }
    std::string text = ImGui::GetCurrentContext()->LogBuffer.c_str();
    ImGui::LogFinish();
    ImGui::End();
    ImGui::Render();
    return text;
}
int main() {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1280, 800);
    io.DeltaTime = 1.0f / 60;
    unsigned char* pixels; int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    GameStats stats;
    for (int tab = 0; tab < 4; ++tab) draw(stats, tab);
    auto summary = draw(stats, 0);
    assert(summary.find("0%") == std::string::npos); // Unknown HP isn't zero health.
    assert(std::regex_search(summary, std::regex(R"(HP[\s|{}]*-)")));
    assert(std::regex_search(summary, std::regex(R"(alerts[\s|{}]*-)")));
    assert(summary.find("+0 (area)") != std::string::npos);
    stats.pw_mission_id = 7;
    stats.pw_cur_rank = 0;
    stats.pw_cur_best = 18345;
    stats.pw_player_hp = 4500;
    stats.pw_player_max_hp = 9000;
    summary = draw(stats, 0);
    assert(summary.find("Best rank S") != std::string::npos);
    assert(summary.find("Best time 1:01.150") != std::string::npos);
    assert(summary.find("50% (4500/9000)") != std::string::npos);
    stats.pw_insignias = 110;
    stats.pw_headshots = 1000000;
    assert(draw(stats, 2).find("110 / 110 insignias earned") != std::string::npos);
    stats.pw_codename_axes_ok = true;
    for (int slot = 0; slot < 12; ++slot) stats.pw_codename_axes[1][slot] = 10;
    auto codenames = draw(stats, 3);
    assert(codenames.find("120") != std::string::npos);
    assert(codenames.find("100%") != std::string::npos);
    assert(codenames.find("109%") == std::string::npos); // Ungrouped slot belongs in lethality denominator.
    assert(codenames.find("Grade pending mission evaluation") != std::string::npos);
    stats.pw_codename_result_ok = true;
    assert(draw(stats, 3).find("Projected grade") != std::string::npos);
    for (int scroll : {0, 1, 1}) draw(stats, 1, scroll);
    bool scrolled = false;
    for (auto* window : ImGui::GetCurrentContext()->Windows)
        if (std::strstr(window->Name, "pw_career_scroll")) scrolled |= window->Scroll.y > 0;
    assert(scrolled);
    draw(stats, 1, -1);
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
print("PW overlay smoke check passed")
