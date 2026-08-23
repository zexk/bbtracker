#include "overlay.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <MinHook.h>

#include <cstdint>
#include <cstdio>
#include <vector>
#include <filesystem>

#include "../common/codename/codename.h"
#include "../common/config.h"
#include "../common/log.h"

namespace bb {
namespace {

struct OverlayState {
    bool imgui_ready = false;
    bool show = true;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    HWND hwnd = nullptr;
};

const char* g_label = "?";
StatsFn g_stats_fn = nullptr;
Game g_game = Game::MGS3;
OverlayState g{};

using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn =
    HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

PresentFn oPresent = nullptr;
ResizeBuffersFn oResizeBuffers = nullptr;

constexpr UINT kPresentIndex = 8;
constexpr UINT kResizeBuffersIndex = 13;

HMODULE own_module()
{
    HMODULE mod = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&own_module), &mod);
    return mod;
}

void poll_toggle_key()
{
    static bool prev_down = false;
    const bool down = (GetAsyncKeyState(config().toggle_key) & 0x8000) != 0;
    if (down && !prev_down) {
        g.show = !g.show;
    }
    prev_down = down;
}

void release_rtv()
{
    if (g.rtv) {
        g.rtv->Release();
        g.rtv = nullptr;
    }
}

bool create_rtv(IDXGISwapChain* swap_chain)
{
    ID3D11Texture2D* back_buffer = nullptr;
    if (FAILED(swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer)))
        || !back_buffer) {
        return false;
    }
    HRESULT hr = g.device->CreateRenderTargetView(back_buffer, nullptr, &g.rtv);
    back_buffer->Release();
    return SUCCEEDED(hr);
}

bool init_imgui(IDXGISwapChain* swap_chain)
{
    if (FAILED(swap_chain->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&g.device)))
        || !g.device) {
        return false;
    }
    g.device->GetImmediateContext(&g.context);

    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swap_chain->GetDesc(&desc))) {
        return false;
    }
    g.hwnd = desc.OutputWindow;

    if (!create_rtv(swap_chain)) {
        return false;
    }

    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplWin32_Init(g.hwnd);
    ImGui_ImplDX11_Init(g.device, g.context);
    g.imgui_ready = true;

    LOG_INFO("overlay ready on hwnd %p", reinterpret_cast<void*>(g.hwnd));
    return true;
}

const char* difficulty_name(Difficulty d)
{
    switch (d) {
    case Difficulty::VeryEasy: return "Very Easy";
    case Difficulty::Easy: return "Easy";
    case Difficulty::Normal: return "Normal";
    case Difficulty::Hard: return "Hard";
    case Difficulty::Extreme: return "Extreme";
    case Difficulty::EuroExtreme: return "Euro Extreme";
    default: return "?";
    }
}

const char* mission_name(int mission)
{
    switch (mission) {
    case 16: return "Tanker";
    case 0: return "Plant";
    case 32: return "Tanker-Plant";
    default: return "?";
    }
}

void format_time(double seconds, char* buf, size_t len)
{
    const int total = static_cast<int>(seconds);
    snprintf(buf, len, "%d:%02d:%02d", total / 3600, (total / 60) % 60, total % 60);
}

void stat_row(const char* key, const char* value)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(key);
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(value);
}

void draw_panel()
{
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = config().scale;

    GameStats stats{};
    const bool have_stats = g_stats_fn && g_stats_fn(stats);

    ImGui::SetNextWindowSize(ImVec2(380, 480), ImGuiCond_FirstUseEver);
    ImGui::Begin("bbtracker", &g.show, ImGuiWindowFlags_NoCollapse);

    if (!have_stats) {
        ImGui::TextDisabled("stats unavailable");
        ImGui::TextDisabled("memory probe not resolved yet; see bbtracker.log");
        ImGui::End();
        return;
    }

    auto match = g_game == Game::MGS2 ? codename::evaluate_mgs2(stats)
                                      : codename::evaluate_mgs3(stats);

    const ImVec4 green(0.42f, 0.90f, 0.45f, 1.0f);
    ImGui::SetWindowFontScale(2.0f);
    ImGui::TextColored(match ? green : ImVec4(1, 1, 1, 0.35f), "%s", match ? match->name : "---");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Text("difficulty: %s%s", difficulty_name(stats.difficulty),
                config().difficulty_override < 0 && stats.difficulty_game_byte % 10 != 0 ? " (?)" : "");
    if (g_game == Game::MGS2) {
        ImGui::Text("mission: %s", mission_name(stats.mission));
    } else if (stats.area_code[0]) {
        ImGui::Text("area: %s", stats.area_code);
    }

    ImGui::Spacing();
    ImGui::TextDisabled("stats");
    if (ImGui::BeginTable("stats", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("stat", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        char buf[64];

        snprintf(buf, sizeof(buf), "%d", stats.kills);
        stat_row("kills", buf);
        snprintf(buf, sizeof(buf), "%d", stats.alerts);
        stat_row("alerts", buf);
        snprintf(buf, sizeof(buf), "%d pts (~%.1f bars)", stats.damage_taken_units,
                 stats.damage_taken_units / 48.0);
        stat_row("damage taken", buf);
        format_time(stats.play_time_seconds, buf, sizeof(buf));
        stat_row("play time", buf);
        snprintf(buf, sizeof(buf), "%d", stats.continues);
        stat_row("continues", buf);
        snprintf(buf, sizeof(buf), "%d", stats.saves);
        stat_row("saves", buf);

        if (g_game == Game::MGS2) {
            snprintf(buf, sizeof(buf), "%d", stats.shots_fired);
            stat_row("shots fired", buf);
            snprintf(buf, sizeof(buf), "%d", stats.rations_used);
            stat_row("rations used", buf);
            stat_row("special items", stats.special_item_used ? "USED" : "not used");
        } else {
            snprintf(buf, sizeof(buf), "%d", stats.severe_injuries);
            stat_row("severe injuries", buf);
            snprintf(buf, sizeof(buf), "%d", stats.life_med_used);
            stat_row("life medicine", buf);
            snprintf(buf, sizeof(buf), "%d", stats.meals_eaten);
            stat_row("meals eaten", buf);
            snprintf(buf, sizeof(buf), "%d / 48", stats.plants_captured);
            stat_row("captures", buf);
            stat_row("special items", stats.special_item_used ? "USED" : "not used");
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    const char* tracker_title = g_game == Game::MGS2 ? "BIG BOSS tracker" : "FOXHOUND tracker";
    if (ImGui::CollapsingHeader(tracker_title, ImGuiTreeNodeFlags_DefaultOpen)) {
        std::vector<codename::ReqStatus> reqs = g_game == Game::MGS2
            ? codename::elite_requirements_mgs2(stats)
            : codename::elite_requirements_mgs3(stats);
        if (ImGui::BeginTable("reqs", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("requirement", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            for (const codename::ReqStatus& r : reqs) {
                char ratio[48];
                switch (static_cast<codename::ReqFmt>(r.fmt)) {
                case codename::ReqFmt::Time: {
                    char cur[16];
                    format_time(r.current * 3600.0, cur, sizeof(cur));
                    snprintf(ratio, sizeof(ratio), "%s / %.0fh", cur, r.limit);
                    break;
                }
                case codename::ReqFmt::Bars:
                    if (r.limit == 0) {
                        snprintf(ratio, sizeof(ratio), "%.1f", static_cast<double>(r.current));
                    } else {
                        snprintf(ratio, sizeof(ratio), "%.1f / %.0f", static_cast<double>(r.current),
                                 r.limit);
                    }
                    break;
                default:
                    if (r.limit == 0) {
                        snprintf(ratio, sizeof(ratio), "%.0f", r.current);
                    } else {
                        snprintf(ratio, sizeof(ratio), "%.0f / %.0f", r.current, r.limit);
                    }
                    break;
                }

                const bool over = !r.pass;
                const bool near_limit = !over && r.limit != 0 && r.current >= r.limit * 0.75;
                const ImVec4 state_col = over ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f)
                                              : near_limit ? ImVec4(1.0f, 0.82f, 0.25f, 1.0f)
                                                           : ImVec4(0.4f, 0.9f, 0.5f, 1.0f);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(r.label);
                ImGui::TableNextColumn();
                ImGui::TextColored(state_col, "%s", ratio);
            }
            ImGui::EndTable();
        }
    }

    ImGui::End();
}

HRESULT STDMETHODCALLTYPE hk_present(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags)
{
    poll_toggle_key();
    if (!g.show) {
        return oPresent(swap_chain, sync_interval, flags);
    }
    if (!g.imgui_ready && !init_imgui(swap_chain)) {
        return oPresent(swap_chain, sync_interval, flags);
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    draw_panel();
    ImGui::Render();

    g.context->OMSetRenderTargets(1, &g.rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    return oPresent(swap_chain, sync_interval, flags);
}

HRESULT STDMETHODCALLTYPE hk_resize_buffers(IDXGISwapChain* swap_chain, UINT buffer_count, UINT width,
                                            UINT height, DXGI_FORMAT format, UINT flags)
{
    if (g.imgui_ready) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
        release_rtv();
    }
    HRESULT hr = oResizeBuffers(swap_chain, buffer_count, width, height, format, flags);
    if (SUCCEEDED(hr) && g.imgui_ready && !g.rtv) {
        create_rtv(swap_chain);
    }
    return hr;
}

bool install_hooks()
{
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = "BBTrackerDummyWnd";
    if (!RegisterClassExA(&wc)) {
        LOG_ERROR("RegisterClassExA failed");
        return false;
    }
    HWND wnd = CreateWindowExA(0, wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100,
                               nullptr, nullptr, wc.hInstance, nullptr);

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = wnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL got{};
    IDXGISwapChain* sc = nullptr;
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;

    auto create = reinterpret_cast<decltype(&D3D11CreateDeviceAndSwapChain)>(
        GetProcAddress(GetModuleHandleW(L"d3d11.dll"), "D3D11CreateDeviceAndSwapChain"));
    HRESULT hr = create ? create(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 1,
                                 D3D11_SDK_VERSION, &sd, &sc, &dev, &got, &ctx)
                        : E_FAIL;
    if (FAILED(hr) || !sc || !dev || !ctx) {
        LOG_ERROR("dummy d3d11 device creation failed hr=0x%08lX", static_cast<unsigned long>(hr));
        if (sc) sc->Release();
        if (dev) dev->Release();
        if (ctx) ctx->Release();
        if (wnd) DestroyWindow(wnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return false;
    }

    void** vtable = *reinterpret_cast<void***>(sc);
    void* present_target = vtable[kPresentIndex];
    void* resize_target = vtable[kResizeBuffersIndex];

    sc->Release();
    dev->Release();
    ctx->Release();
    if (wnd) DestroyWindow(wnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);

    if (MH_Initialize() != MH_OK) {
        LOG_ERROR("MH_Initialize failed");
        return false;
    }
    if (MH_CreateHook(present_target, reinterpret_cast<void*>(&hk_present),
                      reinterpret_cast<void**>(&oPresent))
            != MH_OK
        || MH_CreateHook(resize_target, reinterpret_cast<void*>(&hk_resize_buffers),
                         reinterpret_cast<void**>(&oResizeBuffers))
               != MH_OK) {
        LOG_ERROR("MH_CreateHook failed");
        return false;
    }
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        LOG_ERROR("MH_EnableHook failed");
        return false;
    }
    return true;
}

std::filesystem::path dll_dir()
{
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(own_module(), path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

} // namespace

void start_overlay(const char* game_label, StatsFn stats_fn, const wchar_t* game_module, Game game)
{
    g_label = game_label;
    g_stats_fn = stats_fn;
    g_game = game;

    std::filesystem::path dir = dll_dir();
    std::string ini_path = (dir / L"bbtracker.ini").string();
    std::string log_path = (dir / L"bbtracker.log").string();

    if (!log_init(log_path.c_str())) {
        return;
    }
    Config cfg{};
    if (!load_config(ini_path.c_str(), cfg)) {
        LOG_INFO("no %s found, using defaults", ini_path.c_str());
    }
    g.show = config().visible_default;

    LOG_INFO("bbtracker starting (%s), toggle key vk=0x%02X", g_label, config().toggle_key);

    bool logged_wait_module = false;
    bool logged_wait_d3d11 = false;
    for (;;) {
        if (!GetModuleHandleW(game_module)) {
            if (!logged_wait_module) {
                LOG_INFO("waiting for game module %S", game_module);
                logged_wait_module = true;
            }
            Sleep(500);
            continue;
        }
        if (!GetModuleHandleW(L"d3d11.dll")) {
            if (!logged_wait_d3d11) {
                LOG_INFO("waiting for d3d11.dll");
                logged_wait_d3d11 = true;
            }
            Sleep(500);
            continue;
        }
        break;
    }

    while (!install_hooks()) {
        LOG_WARN("hook install failed, retrying in 5s");
        Sleep(5000);
    }

    LOG_INFO("hooks installed");
}

} // namespace bb
