#include "overlay.h"

#include <kiero.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <filesystem>

#include "../common/config.h"
#include "../common/log.h"

namespace bb {
namespace {

using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn =
    HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

struct OverlayState {
    bool imgui_ready = false;
    bool show = true;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    HWND hwnd = nullptr;
};

const char* g_label = "?";
OverlayState g{};
PresentFn oPresent = nullptr;
ResizeBuffersFn oResizeBuffers = nullptr;

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

void draw_panel()
{
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = config().scale;

    ImGui::SetNextWindowSize(ImVec2(340, 260), ImGuiCond_FirstUseEver);
    ImGui::Begin("bbtracker", &g.show, ImGuiWindowFlags_NoCollapse);
    ImGui::Text("%s", g_label);
    ImGui::Separator();
    ImGui::TextDisabled("codename: --");
    ImGui::Spacing();

    ImGui::BeginTable("stats", 2, ImGuiTableFlags_RowBg);
    ImGui::TableSetupColumn("stat", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableHeadersRow();
    const char* rows[] = { "kills",     "alerts",     "rations used", "saves",
                           "continues", "shots fired", "damage taken", "play time" };
    for (const char* r : rows) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(r);
        ImGui::TableNextColumn();
        ImGui::TextDisabled("--");
    }
    ImGui::EndTable();

    ImGui::Separator();
    ImGui::TextDisabled("rank tracker: --");

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

std::filesystem::path dll_dir()
{
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(own_module(), path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

} // namespace

void start_overlay(const char* game_label)
{
    g_label = game_label;

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

    if (kiero::init(kiero::RenderType::D3D11) != kiero::Status::Success) {
        LOG_ERROR("kiero init failed: no D3D11 device found yet");
        return;
    }
    if (kiero::bind(8, reinterpret_cast<void**>(&oPresent), reinterpret_cast<void*>(&hk_present))
        != kiero::Status::Success) {
        LOG_ERROR("failed to hook Present");
        return;
    }
    kiero::bind(13, reinterpret_cast<void**>(&oResizeBuffers), reinterpret_cast<void*>(&hk_resize_buffers));

    LOG_INFO("hooks installed");
}

} // namespace bb
