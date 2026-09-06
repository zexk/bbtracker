#!/usr/bin/env python3
"""Render the PW overlay panels to PNGs so the layout can be looked at.

Compiles the real draw functions and theme out of src/overlay/overlay.cpp
against ImGui's null backend, rasterizes the draw lists in software over a
stand-in game backdrop, and writes one PNG per tab.

Run with IMGUI_DIR and a native CXX set:
    IMGUI_DIR=... python3 scripts/mockup-pw-overlay.py [outdir]
"""
import os
from pathlib import Path
import shlex
import struct
import subprocess
import sys
import tempfile
import zlib

root = Path(__file__).resolve().parents[1]
imgui = Path(os.environ["IMGUI_DIR"])
out_dir = Path(sys.argv[1] if len(sys.argv) > 1 else root / "mockups")
source = (root / "src/overlay/overlay.cpp").read_text()


def block(start, end):
    return source[source.index(start):source.index(end)]


code = r'''
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "common/codename/codename.h"
#include "overlay/overlay.h"
using namespace bb;
bool (*g_clock_fn)(uint32_t&) = nullptr;
Game g_game = Game::MGSPW;
const char* mgspw_area_name(const char*, int) { return "El Cenagal: Jungle"; }
'''
code += block("void apply_game_theme()\n{", "bool init_imgui(IDXGISwapChain* swap_chain)")
code += block("struct IdColors", "void checklist")
code += block("void draw_mgspw_run", "void draw_panel")
code += r'''
// --- software rasterizer -------------------------------------------------
// Enough of a renderer to look at: bilinear-sampled textured triangles with
// interpolated vertex colour, alpha blended in the same non-linear space the
// real backends blend in.
struct Canvas {
    int w = 0, h = 0;
    std::vector<float> px;  // rgb, opaque
    void init(int width, int height) { w = width; h = height; px.assign(size_t(w) * h * 3, 0.0f); }
};

void sample(const ImTextureData* tex, float u, float v, float out[4])
{
    out[0] = out[1] = out[2] = out[3] = 1.0f;
    if (!tex || !tex->Pixels) return;
    const float x = u * tex->Width - 0.5f, y = v * tex->Height - 0.5f;
    const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
    const float fx = x - x0, fy = y - y0;
    for (int c = 0; c < 4; ++c) out[c] = 0.0f;
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            const int sx = std::clamp(x0 + dx, 0, tex->Width - 1);
            const int sy = std::clamp(y0 + dy, 0, tex->Height - 1);
            const unsigned char* p = tex->Pixels + (size_t(sy) * tex->Width + sx) * tex->BytesPerPixel;
            const float weight = (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy);
            if (tex->BytesPerPixel == 1) {
                for (int c = 0; c < 3; ++c) out[c] += weight;
                out[3] += weight * p[0] / 255.0f;
            } else {
                for (int c = 0; c < 4; ++c) out[c] += weight * p[c] / 255.0f;
            }
        }
    }
}

void raster(Canvas& canvas, const ImDrawData* draw_data)
{
    for (int list_index = 0; list_index < draw_data->CmdListsCount; ++list_index) {
        const ImDrawList* list = draw_data->CmdLists[list_index];
        for (const ImDrawCmd& cmd : list->CmdBuffer) {
            if (cmd.UserCallback) continue;
            const ImTextureData* tex = cmd.TexRef._TexData;
            const ImVec4 clip = cmd.ClipRect;
            for (unsigned i = 0; i < cmd.ElemCount; i += 3) {
                const ImDrawVert* v[3];
                for (int k = 0; k < 3; ++k)
                    v[k] = &list->VtxBuffer[list->IdxBuffer[cmd.IdxOffset + i + k] + cmd.VtxOffset];
                const float area = (v[1]->pos.x - v[0]->pos.x) * (v[2]->pos.y - v[0]->pos.y)
                                 - (v[2]->pos.x - v[0]->pos.x) * (v[1]->pos.y - v[0]->pos.y);
                if (area == 0.0f) continue;
                float min_x = std::min({v[0]->pos.x, v[1]->pos.x, v[2]->pos.x});
                float max_x = std::max({v[0]->pos.x, v[1]->pos.x, v[2]->pos.x});
                float min_y = std::min({v[0]->pos.y, v[1]->pos.y, v[2]->pos.y});
                float max_y = std::max({v[0]->pos.y, v[1]->pos.y, v[2]->pos.y});
                const int x0 = std::max(int(std::floor(std::max(min_x, clip.x))), 0);
                const int x1 = std::min(int(std::ceil(std::min(max_x, clip.z))), canvas.w);
                const int y0 = std::max(int(std::floor(std::max(min_y, clip.y))), 0);
                const int y1 = std::min(int(std::ceil(std::min(max_y, clip.w))), canvas.h);
                for (int y = y0; y < y1; ++y) {
                    for (int x = x0; x < x1; ++x) {
                        const float px = x + 0.5f, py = y + 0.5f;
                        float w[3];
                        w[0] = ((v[1]->pos.x - px) * (v[2]->pos.y - py)
                                - (v[2]->pos.x - px) * (v[1]->pos.y - py)) / area;
                        w[1] = ((v[2]->pos.x - px) * (v[0]->pos.y - py)
                                - (v[0]->pos.x - px) * (v[2]->pos.y - py)) / area;
                        w[2] = 1.0f - w[0] - w[1];
                        if (w[0] < 0 || w[1] < 0 || w[2] < 0) continue;
                        float color[4] = {0, 0, 0, 0}, uv[2] = {0, 0};
                        for (int k = 0; k < 3; ++k) {
                            const ImU32 packed = v[k]->col;
                            const float rgba[4] = {
                                ((packed >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
                                ((packed >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
                                ((packed >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
                                ((packed >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f};
                            for (int c = 0; c < 4; ++c) color[c] += w[k] * rgba[c];
                            uv[0] += w[k] * v[k]->uv.x;
                            uv[1] += w[k] * v[k]->uv.y;
                        }
                        float texel[4];
                        sample(tex, uv[0], uv[1], texel);
                        const float alpha = color[3] * texel[3];
                        if (alpha <= 0.0f) continue;
                        float* dst = &canvas.px[(size_t(y) * canvas.w + x) * 3];
                        for (int c = 0; c < 3; ++c)
                            dst[c] = color[c] * texel[c] * alpha + dst[c] * (1 - alpha);
                    }
                }
            }
        }
    }
}

// Stand-in for the game behind the overlay: a lit-from-the-left jungle-ish
// gradient with a bright band, so translucency and contrast are visible
// against both a dark and a light backdrop.
void backdrop(Canvas& canvas)
{
    for (int y = 0; y < canvas.h; ++y) {
        for (int x = 0; x < canvas.w; ++x) {
            const float u = float(x) / canvas.w, v = float(y) / canvas.h;
            const float band = std::exp(-40.0f * (v - 0.62f) * (v - 0.62f));
            float* dst = &canvas.px[(size_t(y) * canvas.w + x) * 3];
            dst[0] = 0.10f + 0.42f * u * u + 0.30f * band;
            dst[1] = 0.13f + 0.46f * u * u + 0.34f * band;
            dst[2] = 0.09f + 0.34f * u * u + 0.22f * band;
        }
    }
}

// Cropped to the panel plus a margin of backdrop, so each tab's image is the
// size of what that tab draws.
void write_ppm(const Canvas& canvas, const char* path, int crop_w, int crop_h)
{
    const int w = std::min(crop_w, canvas.w), h = std::min(crop_h, canvas.h);
    FILE* file = fopen(path, "wb");
    fprintf(file, "P6\n%d %d\n255\n", w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float* src = &canvas.px[(size_t(y) * canvas.w + x) * 3];
            for (int c = 0; c < 3; ++c)
                fputc((unsigned char)std::clamp(src[c] * 255.0f + 0.5f, 0.0f, 255.0f), file);
        }
    }
    fclose(file);
}

// --- the panel, as draw_panel() frames it --------------------------------
ImVec2 g_panel_size;

void draw_window(const GameStats& stats, int tab)
{
    ImGui::SetNextWindowPos(ImVec2(24, 24));
    ImGui::SetNextWindowSizeConstraints(ImVec2(420, 0), ImVec2(420, FLT_MAX));
    bool open = true;
    ImGui::Begin("FOXHOUND tracker", &open,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
    if (ImGui::BeginTabBar("tracker_tabs")) {
        const char* names[] = {"Summary", "Career", "Insignia", "Codenames"};
        for (int i = 0; i < 4; ++i) {
            if (ImGui::BeginTabItem(names[i], nullptr,
                                    i == tab ? ImGuiTabItemFlags_SetSelected : 0)) {
                switch (i) {
                case 0: draw_mgspw_summary(stats); break;
                case 1: draw_mgspw_global(stats, 0); break;
                case 2: draw_mgspw_insignia(stats); break;
                case 3: draw_mgspw_codenames(stats); break;
                }
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    g_panel_size = ImGui::GetWindowSize();
    ImGui::End();
}

// A save the probe has only partly resolved: what the panels look like on a
// fresh profile, before the descriptors are found and with nothing earned.
GameStats fresh_profile()
{
    GameStats stats{};
    stats.pw_stage_play = 300 * 34;
    stats.pw_total_play = 41 * 60;
    std::strcpy(stats.pw_stage, "w01s03a");
    stats.pw_in_mission = true;
    stats.pw_mission_id = 3;
    stats.pw_player_hp = 6600;
    stats.pw_player_max_hp = 8000;
    stats.pw_heroism = 210;
    stats.pw_gmp = 12500;
    return stats;
}

// A mid-run profile: a mission underway on a career that is most of the
// way to FOXHOUND, so every state the panels colour for is on screen.
GameStats mid_profile()
{
    GameStats stats{};
    stats.pw_stage_play = 300 * 227 + 140;
    stats.pw_total_play = 41 * 3600 + 12 * 60;
    std::strcpy(stats.pw_stage, "w01s04a");
    stats.pw_in_mission = true;
    stats.pw_region_id = 3;
    stats.pw_mission_id = 42;
    stats.pw_cur_rank = 1;
    stats.pw_cur_best = 300 * 194;
    stats.pw_player_hp = 5100;
    stats.pw_player_max_hp = 7800;
    stats.pw_m_kills = 2;
    stats.pw_m_headshots = 6;
    stats.pw_m_alerts = 0;
    stats.pw_m_tranq = 11;
    stats.seg_kills = 2;
    stats.seg_headshots = 6;
    stats.seg_tranq = 11;
    stats.seg_heroism = 1450;
    stats.pw_heroism = 138400;
    stats.pw_heroism_delta = 1450;
    stats.pw_gmp = 421900;
    stats.pw_camaraderie = 12800;
    stats.pw_clears = 316;
    stats.pw_unique_cleared = 121;
    stats.pw_s_missions = 74;
    stats.pw_kills = 388;
    stats.pw_tranq = 1642;
    stats.pw_headshots = 921;
    stats.pw_alerts = 96;
    stats.pw_stealth_kills = 141;
    stats.pw_fulton_recoveries = 407;
    stats.pw_prisoner_extractions = 58;
    stats.pw_holdups = 233;
    stats.pw_nokill_clears = 88;
    stats.pw_noalert_clears = 71;
    stats.pw_noitem_clears = 214;
    stats.pw_damage_taken = 264000;
    stats.pw_pistol_lethal = 44;
    stats.pw_pistol_takedowns = 612;
    stats.pw_ar_takedowns = 173;
    stats.pw_sniper_takedowns = 21;
    stats.pw_sniper_nonlethal = 318;
    stats.pw_lmg_takedowns = 37;
    stats.pw_shotgun_takedowns = 12;
    stats.pw_cqc_takedowns = 496;
    stats.pw_grenade_takedowns = 31;
    stats.pw_rocket_takedowns = 46;
    stats.pw_placed_takedowns = 24;
    stats.pw_insignias = 87;
    stats.pw_codename_axes_ok = true;
    const int sleeps[12] = {410, 96, 0, 214, 33, 0, 0, 0, 289, 0, 0, 12};
    const int kills[12] = {38, 121, 14, 9, 0, 44, 0, 0, 0, 27, 0, 0};
    const int stuns[12] = {0, 0, 0, 0, 0, 0, 0, 0, 121, 0, 0, 0};
    for (int slot = 0; slot < 12; ++slot) {
        stats.pw_codename_axes[0][slot] = kills[slot];
        stats.pw_codename_axes[1][slot] = sleeps[slot];
        stats.pw_codename_axes[2][slot] = stuns[slot];
    }
    stats.pw_codename_state_ok = true;
    for (int id = 1; id <= 17; ++id) stats.pw_codename_state[id] = 1;
    stats.pw_codename_result_ok = true;
    stats.pw_codename_missions_required = 20;
    stats.pw_codename_missions_counted = 17;
    return stats;
}

// The end state: FOXHOUND at grade 5, on a clean run, every family done.
GameStats elite_profile()
{
    GameStats stats = mid_profile();
    stats.pw_stage_play = 300 * 96;
    stats.pw_cur_rank = 0;
    stats.pw_cur_best = 300 * 118;
    stats.pw_player_hp = 8000;
    stats.pw_player_max_hp = 8000;
    stats.pw_m_kills = 0;
    stats.pw_m_headshots = 0;
    stats.pw_m_tranq = 24;
    stats.pw_kills = 0;
    stats.pw_body_kills = 0;
    stats.pw_heroism = 892000;
    stats.pw_camaraderie = 640000;
    stats.pw_insignias = 110;
    stats.pw_holdups = 1400;
    stats.pw_headshots = 4200;
    stats.pw_fulton_recoveries = 2100;
    stats.pw_nokill_clears = 600;
    stats.pw_noalert_clears = 600;
    stats.pw_noitem_clears = 600;
    // Every class carried, non-lethal throughout: the "all weapons" spread
    // FOXHOUND is keyed on.
    for (int slot = 0; slot < 12; ++slot) {
        stats.pw_codename_axes[0][slot] = 0;
        stats.pw_codename_axes[1][slot] = 300;
        stats.pw_codename_axes[2][slot] = 120;
    }
    for (int id = 1; id <= 24; ++id) stats.pw_codename_state[id] = 1;
    stats.pw_codename_grade4_ok = true;
    stats.pw_codename_grade5_ok = true;
    stats.pw_codename_missions_counted = 40;
    return stats;
}

int main(int argc, char** argv)
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(480, 900);
    io.DeltaTime = 1.0f / 60;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    apply_game_theme();

    const GameStats profiles[] = {fresh_profile(), mid_profile(), elite_profile()};
    for (int profile = 0; profile < 3; ++profile) {
    const GameStats& stats = profiles[profile];
    for (int tab = 0; tab < 4; ++tab) {
        Canvas canvas;
        canvas.init(int(io.DisplaySize.x), int(io.DisplaySize.y));
        backdrop(canvas);
        // Tab selection lands on the second frame and the auto-resize to the
        // selected tab's content on the third, so settle for four.
        for (int frame = 0; frame < 4; ++frame) {
            ImGui::NewFrame();
            draw_window(stats, tab);
            ImGui::Render();
            for (ImTextureData* tex : *ImGui::GetDrawData()->Textures) {
                if (tex->Status != ImTextureStatus_OK) tex->SetTexID(1);
                tex->SetStatus(ImTextureStatus_OK);
            }
        }
        raster(canvas, ImGui::GetDrawData());
        char path[512];
        snprintf(path, sizeof(path), "%s/%d-%d.ppm", argv[1], profile, tab);
        write_ppm(canvas, path, int(g_panel_size.x) + 48, int(g_panel_size.y) + 48);
    }
    }
    ImGui::DestroyContext();
    (void)argc;
}
'''


def png(path, width, height, rgb, scale):
    rows = bytearray()
    for y in range(height):
        for _ in range(scale):
            rows.append(0)
            row = rgb[y * width * 3:(y + 1) * width * 3]
            for x in range(width):
                rows.extend(row[x * 3:x * 3 + 3] * scale)

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data)))

    header = struct.pack(">IIBBBBB", width * scale, height * scale, 8, 2, 0, 0, 0)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
                     + chunk(b"IDAT", zlib.compress(bytes(rows), 6))
                     + chunk(b"IEND", b""))


with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / "mock.cpp").write_text(code)
    subprocess.run(shlex.split(os.environ.get("CXX", "c++")) + [
        "-std=c++20", "-O2", f"-I{imgui}", f"-I{root / 'src'}", str(path / "mock.cpp"),
        *map(str, (root / "src/common/codename").glob("*.cpp")),
        *[str(imgui / name) for name in
          ("imgui.cpp", "imgui_draw.cpp", "imgui_tables.cpp", "imgui_widgets.cpp")],
        "-o", str(path / "mock"),
    ], check=True)
    subprocess.run([str(path / "mock"), str(path)], check=True)
    out_dir.mkdir(parents=True, exist_ok=True)
    for profile, profile_name in enumerate(("fresh", "mid", "elite")):
        for tab, tab_name in enumerate(("summary", "career", "insignia", "codenames")):
            parts = (path / f"{profile}-{tab}.ppm").read_bytes().split(b"\n", 3)
            width, height = (int(value) for value in parts[1].split())
            target = out_dir / f"pw-{profile_name}-{tab_name}.png"
            png(target, width, height, parts[3], 2)
            print(target)
