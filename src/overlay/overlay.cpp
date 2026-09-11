#include "overlay.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

#include <windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <MinHook.h>

#include <cstdlib>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <filesystem>

#include "../common/codename/codename.h"
#include "../common/codename/rules_mgs4.h"
#include "../common/format.h"
#include "../common/log.h"
#include "../games/mgs2/dog_tags.h"
#include "../games/mgspw/names.h"

namespace bb {
namespace {

enum class Renderer { Unknown, D3D11, D3D12 };

struct D3D12Frame {
    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12Resource* buffer = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
    UINT64 fence_value = 0;
};

struct OverlayState {
    bool imgui_ready = false;
    bool show = true;
    Renderer renderer = Renderer::Unknown;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D12Device* device12 = nullptr;
    std::atomic<ID3D12CommandQueue*> queue12 = nullptr;
    ID3D12GraphicsCommandList* command_list12 = nullptr;
    ID3D12DescriptorHeap* rtv_heap12 = nullptr;
    ID3D12DescriptorHeap* srv_heap12 = nullptr;
    ID3D12Fence* fence12 = nullptr;
    HANDLE fence_event12 = nullptr;
    UINT64 next_fence12 = 0;
    std::vector<D3D12Frame> frames12;
    HWND hwnd = nullptr;
    ImVec2 render_size{};
    float ui_scale = 0.0f;
    bool reset_window = false;
};

const char* g_label = "?";
StatsFn g_stats_fn = nullptr;
ClockFn g_clock_fn = nullptr;
Game g_game = Game::MGS3;
OverlayState g{};
GameStats g_stats{};
bool g_have_stats = false;
SRWLOCK g_stats_lock = SRWLOCK_INIT;

using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using ResizeBuffersFn =
    HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using ExecuteCommandListsFn =
    void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);

PresentFn oPresent = nullptr;
ResizeBuffersFn oResizeBuffers = nullptr;
ExecuteCommandListsFn oExecuteCommandLists = nullptr;

constexpr UINT kPresentIndex = 8;
constexpr UINT kResizeBuffersIndex = 13;
constexpr UINT kExecuteCommandListsIndex = 10;
constexpr UINT kToggleKey = VK_F3;
constexpr UINT kTabKey = VK_F4;

bool key_pressed(UINT key)
{
    static bool was_down[256]{};
    const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
    const bool pressed = down && !was_down[key];
    was_down[key] = down;
    return pressed;
}

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
    const bool down = (GetAsyncKeyState(kToggleKey) & 0x8000) != 0;
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

void apply_game_theme();

constexpr float overlay_scale(float render_height, float dpi_scale)
{
    const float resolution_scale = render_height >= 2160.0f ? 2.0f : 1.0f;
    return dpi_scale > resolution_scale ? dpi_scale : resolution_scale;
}

static_assert(overlay_scale(1080.0f, 1.0f) == 1.0f);
static_assert(overlay_scale(2160.0f, 1.0f) == 2.0f);
static_assert(overlay_scale(1080.0f, 1.5f) == 1.5f);

void apply_ui_scale()
{
    const float scale = overlay_scale(g.render_size.y,
                                      ImGui_ImplWin32_GetDpiScaleForHwnd(g.hwnd));
    if (scale == g.ui_scale) return;

    apply_game_theme();
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontScaleDpi = scale;
    style.ScaleAllSizes(scale);
    g.ui_scale = scale;
    g.reset_window = true;
}

// Context setup both backends share; only the renderer init differs.
void begin_imgui()
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
    apply_ui_scale();
    ImGui_ImplWin32_Init(g.hwnd);
}

bool create_rtv(IDXGISwapChain* swap_chain)
{
    ID3D11Texture2D* back_buffer = nullptr;
    if (FAILED(swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back_buffer)))
        || !back_buffer) {
        return false;
    }
    D3D11_TEXTURE2D_DESC desc{};
    back_buffer->GetDesc(&desc);
    HRESULT hr = g.device->CreateRenderTargetView(back_buffer, nullptr, &g.rtv);
    back_buffer->Release();
    if (SUCCEEDED(hr)) {
        g.render_size = {static_cast<float>(desc.Width), static_cast<float>(desc.Height)};
    }
    return SUCCEEDED(hr);
}

// Waits for a fence value, up to timeout_ms. False on timeout: the caller
// must skip the frame that needed the fence instead of blocking Present.
// A saturated GPU (or a removed device) used to hang the render thread
// here without bound.
bool wait_d3d12(D3D12Frame* frame, DWORD timeout_ms)
{
    const UINT64 value = frame ? frame->fence_value : g.next_fence12;
    if (!value || !g.fence12 || g.fence12->GetCompletedValue() >= value) {
        return true;
    }
    if (FAILED(g.fence12->SetEventOnCompletion(value, g.fence_event12))) {
        return false;
    }
    return WaitForSingleObject(g.fence_event12, timeout_ms) == WAIT_OBJECT_0;
}

void release_d3d12()
{
    // Teardown keeps the full wait: nothing may still reference the objects
    // below once they are released.
    wait_d3d12(nullptr, INFINITE);
    if (g.imgui_ready && g.renderer == Renderer::D3D12) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    for (auto& frame : g.frames12) {
        if (frame.buffer) frame.buffer->Release();
        if (frame.allocator) frame.allocator->Release();
    }
    g.frames12.clear();
    if (g.command_list12) g.command_list12->Release();
    if (g.rtv_heap12) g.rtv_heap12->Release();
    if (g.srv_heap12) g.srv_heap12->Release();
    if (g.fence12) g.fence12->Release();
    if (g.fence_event12) CloseHandle(g.fence_event12);
    if (g.device12) g.device12->Release();
    g.command_list12 = nullptr;
    g.rtv_heap12 = nullptr;
    g.srv_heap12 = nullptr;
    g.fence12 = nullptr;
    g.fence_event12 = nullptr;
    g.device12 = nullptr;
    g.next_fence12 = 0;
    g.ui_scale = 0.0f;
    g.imgui_ready = false;
    g.renderer = Renderer::Unknown;
}

void alloc_srv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
               D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
{
    *cpu = g.srv_heap12->GetCPUDescriptorHandleForHeapStart();
    *gpu = g.srv_heap12->GetGPUDescriptorHandleForHeapStart();
}

void free_srv(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE,
              D3D12_GPU_DESCRIPTOR_HANDLE)
{
}

bool init_imgui_d3d12(IDXGISwapChain* swap_chain)
{
    ID3D12CommandQueue* queue = g.queue12.load();
    if (!queue
        || FAILED(swap_chain->GetDevice(__uuidof(ID3D12Device),
                                        reinterpret_cast<void**>(&g.device12)))) {
        return false;
    }

    IDXGISwapChain3* swap_chain3 = nullptr;
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swap_chain->QueryInterface(IID_PPV_ARGS(&swap_chain3)))
        || FAILED(swap_chain->GetDesc(&desc)) || !desc.BufferCount) {
        if (swap_chain3) swap_chain3->Release();
        release_d3d12();
        return false;
    }
    g.hwnd = desc.OutputWindow;
    g.frames12.resize(desc.BufferCount);

    D3D12_DESCRIPTOR_HEAP_DESC rtv_desc{};
    rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_desc.NumDescriptors = desc.BufferCount;
    D3D12_DESCRIPTOR_HEAP_DESC srv_desc{};
    srv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_desc.NumDescriptors = 1;
    srv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    bool ok = SUCCEEDED(g.device12->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&g.rtv_heap12)))
        && SUCCEEDED(g.device12->CreateDescriptorHeap(&srv_desc, IID_PPV_ARGS(&g.srv_heap12)))
        && SUCCEEDED(g.device12->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g.fence12)));
    g.fence_event12 = ok ? CreateEventW(nullptr, FALSE, FALSE, nullptr) : nullptr;
    ok = ok && g.fence_event12;

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = ok ? g.rtv_heap12->GetCPUDescriptorHandleForHeapStart()
                                         : D3D12_CPU_DESCRIPTOR_HANDLE{};
    const UINT step = ok ? g.device12->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
                         : 0;
    for (UINT i = 0; ok && i < desc.BufferCount; ++i, rtv.ptr += step) {
        auto& frame = g.frames12[i];
        frame.rtv = rtv;
        ok = SUCCEEDED(g.device12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                          IID_PPV_ARGS(&frame.allocator)))
            && SUCCEEDED(swap_chain3->GetBuffer(i, IID_PPV_ARGS(&frame.buffer)));
        if (ok) g.device12->CreateRenderTargetView(frame.buffer, nullptr, frame.rtv);
    }
    if (ok) {
        ok = SUCCEEDED(g.device12->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                     g.frames12[0].allocator, nullptr,
                                                     IID_PPV_ARGS(&g.command_list12)));
        if (ok) g.command_list12->Close();
    }
    swap_chain3->Release();
    if (!ok) {
        release_d3d12();
        return false;
    }
    const D3D12_RESOURCE_DESC back_desc = g.frames12[0].buffer->GetDesc();
    g.render_size = {static_cast<float>(back_desc.Width), static_cast<float>(back_desc.Height)};

    begin_imgui();
    ImGui_ImplDX12_InitInfo info{};
    info.Device = g.device12;
    info.CommandQueue = queue;
    info.NumFramesInFlight = static_cast<int>(desc.BufferCount);
    info.RTVFormat = desc.BufferDesc.Format;
    info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    info.SrvDescriptorHeap = g.srv_heap12;
    info.SrvDescriptorAllocFn = alloc_srv;
    info.SrvDescriptorFreeFn = free_srv;
    if (!ImGui_ImplDX12_Init(&info)) {
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        release_d3d12();
        return false;
    }
    g.renderer = Renderer::D3D12;
    g.imgui_ready = true;
    LOG_INFO("D3D12 overlay ready on hwnd %p", reinterpret_cast<void*>(g.hwnd));
    return true;
}

// Every panel is drawn at the same translucency; only the palette changes.
constexpr float kPanelAlpha = 0.80f;

constexpr ImVec4 with_alpha(ImVec4 color, float alpha)
{
    color.w = alpha;
    return color;
}

void apply_game_theme()
{
    ImGui::StyleColorsDark();
    if (g_game != Game::MG1 && g_game != Game::MG2
        && g_game != Game::MGS1 && g_game != Game::MGS2
        && g_game != Game::MGS3 && g_game != Game::MGS4
        && g_game != Game::MGSPW && g_game != Game::Babel) {
        return;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;

    ImVec4* colors = style.Colors;
    if (g_game == Game::MG1) {
        const ImVec4 text{0.72f, 0.73f, 0.72f, 1.0f};
        const ImVec4 dim{0.43f, 0.44f, 0.43f, 1.0f};
        const ImVec4 panel{0.01f, 0.01f, 0.01f, 1.0f};
        const ImVec4 border{0.58f, 0.59f, 0.57f, 1.0f};
        const ImVec4 frame{0.16f, 0.16f, 0.15f, 1.0f};
        const ImVec4 hover{0.40f, 0.18f, 0.04f, 1.0f};
        const ImVec4 active{0.62f, 0.12f, 0.06f, 1.0f};
        const ImVec4 title{0.22f, 0.03f, 0.02f, 1.0f};
        const ImVec4 title_active{0.55f, 0.07f, 0.04f, 1.0f};
        const ImVec4 accent{0.80f, 0.48f, 0.04f, 1.0f};
        const ImVec4 idle{0.20f, 0.20f, 0.19f, 1.0f};

        colors[ImGuiCol_Text]              = text;
        colors[ImGuiCol_TextDisabled]      = dim;
        colors[ImGuiCol_WindowBg]          = with_alpha(panel, kPanelAlpha);
        colors[ImGuiCol_Border]            = with_alpha(border, 0.72f);
        colors[ImGuiCol_FrameBg]           = frame;
        colors[ImGuiCol_FrameBgHovered]    = hover;
        colors[ImGuiCol_FrameBgActive]     = active;
        colors[ImGuiCol_TitleBg]           = title;
        colors[ImGuiCol_TitleBgActive]     = title_active;
        colors[ImGuiCol_CheckMark]         = accent;
        colors[ImGuiCol_SliderGrab]        = border;
        colors[ImGuiCol_SliderGrabActive]  = accent;
        colors[ImGuiCol_Button]            = idle;
        colors[ImGuiCol_ButtonHovered]     = hover;
        colors[ImGuiCol_ButtonActive]      = active;
        colors[ImGuiCol_Header]            = idle;
        colors[ImGuiCol_HeaderHovered]     = hover;
        colors[ImGuiCol_HeaderActive]      = active;
        colors[ImGuiCol_Separator]         = with_alpha(border, 0.68f);
        colors[ImGuiCol_TableRowBgAlt]     = with_alpha(frame, 0.70f);
        colors[ImGuiCol_ResizeGrip]        = with_alpha(active, 0.35f);
        colors[ImGuiCol_ResizeGripHovered] = with_alpha(accent, 0.75f);
        colors[ImGuiCol_ResizeGripActive]  = accent;
        return;
    }

    if (g_game == Game::MGS1) {
        const ImVec4 text{0.55f, 0.76f, 0.69f, 1.0f};
        const ImVec4 dim{0.32f, 0.47f, 0.43f, 1.0f};
        const ImVec4 panel{0.03f, 0.07f, 0.07f, 1.0f};
        const ImVec4 border{0.43f, 0.72f, 0.64f, 1.0f};
        const ImVec4 frame{0.08f, 0.17f, 0.16f, 1.0f};
        const ImVec4 hover{0.20f, 0.43f, 0.38f, 1.0f};
        const ImVec4 active{0.30f, 0.58f, 0.51f, 1.0f};
        const ImVec4 title{0.05f, 0.13f, 0.13f, 1.0f};
        const ImVec4 title_active{0.12f, 0.27f, 0.25f, 1.0f};
        const ImVec4 accent{0.60f, 0.94f, 0.82f, 1.0f};
        const ImVec4 idle{0.10f, 0.23f, 0.21f, 1.0f};
        const ImVec4 row{0.08f, 0.18f, 0.17f, 1.0f};

        colors[ImGuiCol_Text]              = text;
        colors[ImGuiCol_TextDisabled]      = dim;
        colors[ImGuiCol_WindowBg]          = with_alpha(panel, kPanelAlpha);
        colors[ImGuiCol_Border]            = with_alpha(border, 0.65f);
        colors[ImGuiCol_FrameBg]           = frame;
        colors[ImGuiCol_FrameBgHovered]    = hover;
        colors[ImGuiCol_FrameBgActive]     = active;
        colors[ImGuiCol_TitleBg]           = title;
        colors[ImGuiCol_TitleBgActive]     = title_active;
        colors[ImGuiCol_CheckMark]         = accent;
        colors[ImGuiCol_SliderGrab]        = border;
        colors[ImGuiCol_SliderGrabActive]  = accent;
        colors[ImGuiCol_Button]            = idle;
        colors[ImGuiCol_ButtonHovered]     = hover;
        colors[ImGuiCol_ButtonActive]      = active;
        colors[ImGuiCol_Header]            = idle;
        colors[ImGuiCol_HeaderHovered]     = hover;
        colors[ImGuiCol_HeaderActive]      = active;
        colors[ImGuiCol_Separator]         = with_alpha(border, 0.65f);
        colors[ImGuiCol_TableRowBgAlt]     = with_alpha(row, 0.55f);
        colors[ImGuiCol_ResizeGrip]        = with_alpha(border, 0.30f);
        colors[ImGuiCol_ResizeGripHovered] = with_alpha(accent, 0.70f);
        colors[ImGuiCol_ResizeGripActive]  = accent;
        return;
    }

    if (g_game == Game::MG2) {
        const ImVec4 text{0.72f, 0.75f, 0.76f, 1.0f};
        const ImVec4 dim{0.42f, 0.46f, 0.50f, 1.0f};
        const ImVec4 panel{0.00f, 0.01f, 0.03f, 1.0f};
        const ImVec4 border{0.12f, 0.30f, 0.78f, 1.0f};
        const ImVec4 frame{0.03f, 0.07f, 0.17f, 1.0f};
        const ImVec4 hover{0.08f, 0.20f, 0.48f, 1.0f};
        const ImVec4 active{0.12f, 0.30f, 0.70f, 1.0f};
        const ImVec4 title{0.02f, 0.05f, 0.16f, 1.0f};
        const ImVec4 title_active{0.04f, 0.11f, 0.34f, 1.0f};
        const ImVec4 accent{0.42f, 0.62f, 0.94f, 1.0f};
        const ImVec4 idle{0.04f, 0.10f, 0.25f, 1.0f};
        const ImVec4 slider{0.25f, 0.43f, 0.78f, 1.0f};

        colors[ImGuiCol_Text]              = text;
        colors[ImGuiCol_TextDisabled]      = dim;
        colors[ImGuiCol_WindowBg]          = with_alpha(panel, kPanelAlpha);
        colors[ImGuiCol_Border]            = with_alpha(border, 0.80f);
        colors[ImGuiCol_FrameBg]           = frame;
        colors[ImGuiCol_FrameBgHovered]    = hover;
        colors[ImGuiCol_FrameBgActive]     = active;
        colors[ImGuiCol_TitleBg]           = title;
        colors[ImGuiCol_TitleBgActive]     = title_active;
        colors[ImGuiCol_CheckMark]            = accent;
        colors[ImGuiCol_SliderGrab]           = slider;
        colors[ImGuiCol_SliderGrabActive]     = accent;
        colors[ImGuiCol_Button]               = idle;
        colors[ImGuiCol_ButtonHovered]        = hover;
        colors[ImGuiCol_ButtonActive]         = active;
        colors[ImGuiCol_Header]               = idle;
        colors[ImGuiCol_HeaderHovered]        = hover;
        colors[ImGuiCol_HeaderActive]         = active;
        colors[ImGuiCol_Separator]            = with_alpha(border, 0.75f);
        colors[ImGuiCol_TableRowBgAlt]        = with_alpha(frame, 0.72f);
        colors[ImGuiCol_ResizeGrip]           = with_alpha(border, 0.35f);
        colors[ImGuiCol_ResizeGripHovered] = with_alpha(accent, 0.75f);
        colors[ImGuiCol_ResizeGripActive]  = accent;
        return;
    }

    if (g_game == Game::MGS2) {
        style.ScrollbarRounding = 0.0f;
        const ImVec4 text{0.61f, 0.69f, 0.64f, 1.0f};
        const ImVec4 dim{0.34f, 0.42f, 0.38f, 1.0f};
        const ImVec4 panel{0.02f, 0.05f, 0.04f, 1.0f};
        const ImVec4 border{0.38f, 0.53f, 0.47f, 1.0f};
        const ImVec4 frame{0.07f, 0.13f, 0.11f, 1.0f};
        const ImVec4 hover{0.28f, 0.10f, 0.07f, 1.0f};
        const ImVec4 active{0.48f, 0.13f, 0.08f, 1.0f};
        const ImVec4 title{0.04f, 0.09f, 0.07f, 1.0f};
        const ImVec4 title_active{0.20f, 0.08f, 0.06f, 1.0f};
        const ImVec4 accent{0.76f, 0.19f, 0.11f, 1.0f};
        const ImVec4 idle{0.10f, 0.18f, 0.15f, 1.0f};
        const ImVec4 row{0.08f, 0.15f, 0.12f, 1.0f};
        const ImVec4 slider{0.48f, 0.61f, 0.55f, 1.0f};
        const ImVec4 scrollbar{0.23f, 0.34f, 0.29f, 1.0f};

        colors[ImGuiCol_Text]                 = text;
        colors[ImGuiCol_TextDisabled]         = dim;
        colors[ImGuiCol_WindowBg]             = with_alpha(panel, kPanelAlpha);
        colors[ImGuiCol_Border]               = with_alpha(border, 0.70f);
        colors[ImGuiCol_FrameBg]              = frame;
        colors[ImGuiCol_FrameBgHovered]       = hover;
        colors[ImGuiCol_FrameBgActive]        = active;
        colors[ImGuiCol_TitleBg]              = title;
        colors[ImGuiCol_TitleBgActive]        = title_active;
        colors[ImGuiCol_CheckMark]            = accent;
        colors[ImGuiCol_SliderGrab]           = slider;
        colors[ImGuiCol_SliderGrabActive]     = accent;
        colors[ImGuiCol_Button]               = idle;
        colors[ImGuiCol_ButtonHovered]        = hover;
        colors[ImGuiCol_ButtonActive]         = active;
        colors[ImGuiCol_Header]               = idle;
        colors[ImGuiCol_HeaderHovered]        = hover;
        colors[ImGuiCol_HeaderActive]         = active;
        colors[ImGuiCol_Tab]                  = frame;
        colors[ImGuiCol_TabHovered]           = hover;
        colors[ImGuiCol_TabActive]            = active;
        colors[ImGuiCol_TabUnfocused]         = title;
        colors[ImGuiCol_TabUnfocusedActive]   = title_active;
        colors[ImGuiCol_Separator]            = with_alpha(border, 0.70f);
        colors[ImGuiCol_TableRowBgAlt]        = with_alpha(row, 0.55f);
        colors[ImGuiCol_ResizeGrip]           = with_alpha(slider, 0.30f);
        colors[ImGuiCol_ResizeGripHovered]    = with_alpha(accent, 0.70f);
        colors[ImGuiCol_ResizeGripActive]     = accent;
        colors[ImGuiCol_ScrollbarBg]          = with_alpha(title, 0.70f);
        colors[ImGuiCol_ScrollbarGrab]        = with_alpha(scrollbar, 0.85f);
        colors[ImGuiCol_ScrollbarGrabHovered] = with_alpha(active, 0.90f);
        colors[ImGuiCol_ScrollbarGrabActive]  = with_alpha(accent, 0.95f);
        return;
    }

    if (g_game == Game::MGS4) {
        style.ScrollbarRounding = 0.0f;
        const ImVec4 text{0.84f, 0.83f, 0.66f, 1.0f};
        const ImVec4 dim{0.49f, 0.48f, 0.36f, 1.0f};
        const ImVec4 panel{0.08f, 0.08f, 0.05f, 1.0f};
        const ImVec4 border{0.52f, 0.48f, 0.31f, 1.0f};
        const ImVec4 frame{0.17f, 0.16f, 0.10f, 1.0f};
        const ImVec4 hover{0.42f, 0.30f, 0.08f, 1.0f};
        const ImVec4 active{0.62f, 0.42f, 0.08f, 1.0f};
        const ImVec4 title{0.13f, 0.12f, 0.07f, 1.0f};
        const ImVec4 title_active{0.32f, 0.27f, 0.12f, 1.0f};
        const ImVec4 accent{0.94f, 0.66f, 0.14f, 1.0f};
        const ImVec4 idle{0.21f, 0.20f, 0.12f, 1.0f};
        const ImVec4 row{0.20f, 0.19f, 0.12f, 1.0f};
        const ImVec4 slider{0.58f, 0.53f, 0.32f, 1.0f};
        const ImVec4 scrollbar{0.38f, 0.35f, 0.21f, 1.0f};
        const ImVec4 scrollbar_bg{0.10f, 0.10f, 0.06f, 1.0f};

        colors[ImGuiCol_Text]                 = text;
        colors[ImGuiCol_TextDisabled]         = dim;
        colors[ImGuiCol_WindowBg]             = with_alpha(panel, kPanelAlpha);
        colors[ImGuiCol_Border]               = with_alpha(border, 0.72f);
        colors[ImGuiCol_FrameBg]              = frame;
        colors[ImGuiCol_FrameBgHovered]       = hover;
        colors[ImGuiCol_FrameBgActive]        = active;
        colors[ImGuiCol_TitleBg]              = title;
        colors[ImGuiCol_TitleBgActive]        = title_active;
        colors[ImGuiCol_CheckMark]            = accent;
        colors[ImGuiCol_SliderGrab]           = slider;
        colors[ImGuiCol_SliderGrabActive]     = accent;
        colors[ImGuiCol_Button]               = idle;
        colors[ImGuiCol_ButtonHovered]        = hover;
        colors[ImGuiCol_ButtonActive]         = active;
        colors[ImGuiCol_Header]               = idle;
        colors[ImGuiCol_HeaderHovered]        = hover;
        colors[ImGuiCol_HeaderActive]         = active;
        colors[ImGuiCol_Tab]                  = frame;
        colors[ImGuiCol_TabHovered]           = hover;
        colors[ImGuiCol_TabActive]            = active;
        colors[ImGuiCol_TabUnfocused]         = title;
        colors[ImGuiCol_TabUnfocusedActive]   = title_active;
        colors[ImGuiCol_Separator]            = with_alpha(border, 0.68f);
        colors[ImGuiCol_TableRowBgAlt]        = with_alpha(row, 0.60f);
        colors[ImGuiCol_ResizeGrip]           = with_alpha(active, 0.35f);
        colors[ImGuiCol_ResizeGripHovered]    = with_alpha(accent, 0.75f);
        colors[ImGuiCol_ResizeGripActive]     = accent;
        colors[ImGuiCol_ScrollbarBg]          = with_alpha(scrollbar_bg, 0.85f);
        colors[ImGuiCol_ScrollbarGrab]        = with_alpha(scrollbar, 0.90f);
        colors[ImGuiCol_ScrollbarGrabHovered] = with_alpha(active, 0.95f);
        colors[ImGuiCol_ScrollbarGrabActive]  = accent;
        return;
    }

    // Peace Walker's own menus are near-black panels with white rules and one
    // saturated red for anything selected or failing, and its tabs are the
    // only rounded ones in the series.
    if (g_game == Game::MGSPW) {
        style.CellPadding = ImVec2(6.0f, 3.0f);
        style.TabRounding = 5.0f;
        style.ScrollbarRounding = 0.0f;
        const ImVec4 text{0.93f, 0.93f, 0.90f, 1.0f};
        const ImVec4 dim{0.66f, 0.67f, 0.66f, 1.0f};
        const ImVec4 panel{0.05f, 0.05f, 0.06f, 1.0f};
        const ImVec4 border{0.86f, 0.86f, 0.83f, 1.0f};
        const ImVec4 frame{0.11f, 0.11f, 0.12f, 1.0f};
        const ImVec4 hover{0.55f, 0.05f, 0.07f, 1.0f};
        const ImVec4 active{0.86f, 0.06f, 0.09f, 1.0f};
        const ImVec4 title{0.08f, 0.08f, 0.09f, 1.0f};
        const ImVec4 title_active{0.14f, 0.14f, 0.15f, 1.0f};
        const ImVec4 accent{0.94f, 0.94f, 0.92f, 1.0f};
        const ImVec4 header{0.16f, 0.16f, 0.17f, 1.0f};
        const ImVec4 header_hover{0.24f, 0.24f, 0.25f, 1.0f};
        const ImVec4 header_active{0.32f, 0.32f, 0.33f, 1.0f};
        const ImVec4 slider{0.62f, 0.62f, 0.60f, 1.0f};
        const ImVec4 tab{0.10f, 0.10f, 0.11f, 1.0f};
        const ImVec4 tab_unfocused_active{0.45f, 0.04f, 0.06f, 1.0f};
        const ImVec4 grab_active{0.95f, 0.10f, 0.12f, 1.0f};
        const ImVec4 table_strong{0.60f, 0.60f, 0.58f, 1.0f};
        const ImVec4 table_light{0.40f, 0.40f, 0.39f, 1.0f};
        const ImVec4 row{0.13f, 0.13f, 0.14f, 1.0f};
        const ImVec4 scrollbar{0.32f, 0.32f, 0.33f, 1.0f};
        const ImVec4 scrollbar_bg{0.08f, 0.08f, 0.09f, 1.0f};

        colors[ImGuiCol_Text]                 = text;
        colors[ImGuiCol_TextDisabled]         = dim;
        colors[ImGuiCol_WindowBg]             = with_alpha(panel, kPanelAlpha);
        colors[ImGuiCol_Border]               = with_alpha(border, 0.55f);
        colors[ImGuiCol_FrameBg]              = frame;
        colors[ImGuiCol_FrameBgHovered]       = hover;
        colors[ImGuiCol_FrameBgActive]        = active;
        colors[ImGuiCol_TitleBg]              = title;
        // Red is a selection colour in these menus, not a chrome colour: the
        // title bar stays black so the only red plate is the live tab.
        colors[ImGuiCol_TitleBgActive]        = title_active;
        colors[ImGuiCol_CheckMark]            = accent;
        colors[ImGuiCol_SliderGrab]           = slider;
        colors[ImGuiCol_SliderGrabActive]     = active;
        colors[ImGuiCol_Button]               = frame;
        colors[ImGuiCol_ButtonHovered]        = hover;
        colors[ImGuiCol_ButtonActive]         = active;
        // Table header rows: dark plates under white text, not red ones. Red is
        // kept for the selected tab and for a value that is failing.
        colors[ImGuiCol_Header]               = header;
        colors[ImGuiCol_HeaderHovered]        = header_hover;
        colors[ImGuiCol_HeaderActive]         = header_active;
        colors[ImGuiCol_TableHeaderBg]        = header;
        colors[ImGuiCol_Tab]                  = tab;
        colors[ImGuiCol_TabHovered]           = hover;
        colors[ImGuiCol_TabActive]            = active;
        colors[ImGuiCol_TabUnfocused]         = title;
        colors[ImGuiCol_TabUnfocusedActive]   = tab_unfocused_active;
        colors[ImGuiCol_Separator]            = with_alpha(border, 0.45f);
        colors[ImGuiCol_TableBorderStrong]    = with_alpha(table_strong, 0.55f);
        colors[ImGuiCol_TableBorderLight]     = with_alpha(table_light, 0.45f);
        colors[ImGuiCol_TableRowBgAlt]        = with_alpha(row, 0.55f);
        colors[ImGuiCol_ResizeGrip]           = with_alpha(active, 0.35f);
        colors[ImGuiCol_ResizeGripHovered]    = with_alpha(active, 0.75f);
        colors[ImGuiCol_ResizeGripActive]     = grab_active;
        colors[ImGuiCol_ScrollbarBg]          = with_alpha(scrollbar_bg, 0.80f);
        colors[ImGuiCol_ScrollbarGrab]        = with_alpha(scrollbar, 0.90f);
        colors[ImGuiCol_ScrollbarGrabHovered] = with_alpha(hover, 0.95f);
        colors[ImGuiCol_ScrollbarGrabActive]  = active;
        return;
    }

    // Ghost Babel's HUD is a Game Boy Color palette: pure black plates, a
    // one-pixel amber frame with squared corners, white pixel text, the item
    // slots in pure red, and the LIFE bar in teal. Nothing here is tinted or
    // blended — the console had no alpha, so the plates stay flat and only the
    // window itself is translucent enough to play under.
    if (g_game == Game::Babel) {
        // Ghost Babel's title menu: black ground, white text, the live entry in
        // the menu green, the second entry in pure red, and unavailable entries
        // in flat gray. There is no yellow on that screen, so the middle band is
        // inferred from the same flat, fully saturated GBC palette.
        // Only the chrome this panel draws: no tabs, no scrollbar or resize grip
        // under AlwaysAutoResize, no frames, headers or selectables. The button
        // colors are here for the title bar's close box.
        const ImVec4 text{0.97f, 0.97f, 0.97f, 1.0f};
        const ImVec4 dim{0.56f, 0.56f, 0.56f, 1.0f};
        const ImVec4 panel{0.00f, 0.00f, 0.00f, 1.0f};
        const ImVec4 rule{0.00f, 0.63f, 0.36f, 1.0f};
        const ImVec4 title{0.00f, 0.00f, 0.00f, 1.0f};
        const ImVec4 title_active{0.00f, 0.20f, 0.11f, 1.0f};
        const ImVec4 button{0.00f, 0.16f, 0.09f, 1.0f};
        const ImVec4 button_hover{0.00f, 0.42f, 0.24f, 1.0f};
        const ImVec4 row{0.06f, 0.06f, 0.06f, 1.0f};

        colors[ImGuiCol_Text]              = text;
        colors[ImGuiCol_TextDisabled]      = dim;
        colors[ImGuiCol_WindowBg]          = with_alpha(panel, kPanelAlpha);
        colors[ImGuiCol_Border]            = rule;
        colors[ImGuiCol_TitleBg]           = title;
        colors[ImGuiCol_TitleBgActive]     = title_active;
        colors[ImGuiCol_Button]            = button;
        colors[ImGuiCol_ButtonHovered]     = button_hover;
        colors[ImGuiCol_ButtonActive]      = rule;
        colors[ImGuiCol_Separator]         = with_alpha(rule, 0.55f);
        colors[ImGuiCol_TableBorderLight]  = with_alpha(dim, 0.35f);
        colors[ImGuiCol_TableRowBgAlt]     = with_alpha(row, 0.55f);
        return;
    }

    {
        const ImVec4 text{0.86f, 0.87f, 0.76f, 1.0f};
        const ImVec4 dim{0.48f, 0.49f, 0.42f, 1.0f};
        const ImVec4 panel{0.09f, 0.10f, 0.08f, 1.0f};
        const ImVec4 border{0.38f, 0.39f, 0.31f, 1.0f};
        const ImVec4 frame{0.20f, 0.21f, 0.17f, 1.0f};
        const ImVec4 frame_hover{0.29f, 0.30f, 0.24f, 1.0f};
        const ImVec4 frame_active{0.36f, 0.37f, 0.29f, 1.0f};
        const ImVec4 hover{0.36f, 0.37f, 0.28f, 1.0f};
        const ImVec4 active{0.44f, 0.45f, 0.33f, 1.0f};
        const ImVec4 title{0.15f, 0.16f, 0.12f, 1.0f};
        const ImVec4 title_active{0.31f, 0.33f, 0.25f, 1.0f};
        const ImVec4 accent{0.72f, 0.75f, 0.51f, 1.0f};
        const ImVec4 idle{0.25f, 0.26f, 0.20f, 1.0f};
        const ImVec4 slider{0.55f, 0.57f, 0.41f, 1.0f};
        const ImVec4 row{0.22f, 0.23f, 0.19f, 1.0f};
        const ImVec4 scrollbar{0.38f, 0.39f, 0.28f, 1.0f};
        const ImVec4 scrollbar_bg{0.12f, 0.13f, 0.10f, 1.0f};

        style.ScrollbarRounding = 0.0f;
        colors[ImGuiCol_Text]                 = text;
        colors[ImGuiCol_TextDisabled]         = dim;
        colors[ImGuiCol_WindowBg]             = with_alpha(panel, kPanelAlpha);
        colors[ImGuiCol_Border]               = with_alpha(border, 0.65f);
        colors[ImGuiCol_FrameBg]              = frame;
        colors[ImGuiCol_FrameBgHovered]       = frame_hover;
        colors[ImGuiCol_FrameBgActive]        = frame_active;
        colors[ImGuiCol_TitleBg]              = title;
        colors[ImGuiCol_TitleBgActive]        = title_active;
        colors[ImGuiCol_CheckMark]            = accent;
        colors[ImGuiCol_SliderGrab]           = slider;
        colors[ImGuiCol_SliderGrabActive]     = accent;
        colors[ImGuiCol_Button]               = idle;
        colors[ImGuiCol_ButtonHovered]        = hover;
        colors[ImGuiCol_ButtonActive]         = active;
        colors[ImGuiCol_Header]               = idle;
        colors[ImGuiCol_HeaderHovered]        = hover;
        colors[ImGuiCol_HeaderActive]         = active;
        colors[ImGuiCol_Tab]                  = frame;
        colors[ImGuiCol_TabHovered]           = hover;
        colors[ImGuiCol_TabActive]            = active;
        colors[ImGuiCol_TabUnfocused]         = title;
        colors[ImGuiCol_TabUnfocusedActive]   = title_active;
        colors[ImGuiCol_Separator]            = with_alpha(border, 0.65f);
        colors[ImGuiCol_TableRowBgAlt]        = with_alpha(row, 0.55f);
        colors[ImGuiCol_ResizeGrip]           = with_alpha(slider, 0.30f);
        colors[ImGuiCol_ResizeGripHovered]    = with_alpha(accent, 0.70f);
        colors[ImGuiCol_ResizeGripActive]     = accent;
        colors[ImGuiCol_ScrollbarBg]          = with_alpha(scrollbar_bg, 0.70f);
        colors[ImGuiCol_ScrollbarGrab]        = with_alpha(scrollbar, 0.85f);
        colors[ImGuiCol_ScrollbarGrabHovered] = with_alpha(slider, 0.90f);
        colors[ImGuiCol_ScrollbarGrabActive]  = with_alpha(accent, 0.95f);
    }
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

    begin_imgui();
    ImGui_ImplDX11_Init(g.device, g.context);
    g.renderer = Renderer::D3D11;
    g.imgui_ready = true;

    LOG_INFO("D3D11 overlay ready on hwnd %p", reinterpret_cast<void*>(g.hwnd));
    return true;
}

const char* difficulty_name(Difficulty d)
{
    if (g_game == Game::MGS4) {
        switch (d) {
        case Difficulty::VeryEasy: return "Liquid Easy";
        case Difficulty::Easy: return "Naked Normal";
        case Difficulty::Normal: return "Solid Normal";
        case Difficulty::Hard: return "Big Boss Hard";
        case Difficulty::Extreme: return "The Boss Extreme";
        default: return "?";
        }
    }
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

struct AreaName {
    const char* code;
    const char* name;
};

const char* mgs1_area_name(const char* stage)
{
    static constexpr AreaName kAreas[] = {
        {"03a", "Cell"},           {"03b", "Medi Room"},
        {"03c", "Medi Room"},      {"03d", "Cell"},
        {"04a", "Armory"},         {"04b", "Armory South"},
        {"04c", "Armory South"},   {"07a", "Nuke Building B1"},
        {"07b", "Commander's Room"}, {"07c", "Nuke Building B1"},
        {"08a", "Nuke Building B2"}, {"08b", "Lab"},
        {"08c", "Lab Hallway"},    {"11a", "Comms Tower A"},
        {"11b", "Comms Tower A Roof"}, {"11c", "Comms Tower B"},
        {"11d", "Comms Tower A Wall"}, {"11e", "Comms Tower B Elevator"},
        {"11g", "Comms Tower A Roof"}, {"11h", "Comms Tower B Roof"},
        {"11i", "Walkway"},        {"15a", "Warehouse"},
        {"15b", "Warehouse North"}, {"15c", "Warehouse"},
        {"16a", "Underground Base 1"}, {"16b", "Underground Base 2"},
        {"16c", "Underground Base 3"}, {"16d", "Command Room"},
        {"16e", "Underground Base 3"}, {"19a", "Escape Route 1"},
        {"19b", "Escape Route 2"},
        {"00", "Dock"},            {"01", "Heliport"},
        {"02", "Tank Hangar"},     {"05", "Canyon"},
        {"06", "Nuke Building 1F"}, {"09", "Cave"},
        {"10", "Underground Passage"}, {"12", "Snowfield"},
        {"13", "Blast Furnace"},   {"14", "Cargo Elevator"},
        {"17", "Supply Route"},    {"18", "Supply Route"},
    };
    if (!stage || (stage[0] != 's' && stage[0] != 'd')) {
        return nullptr;
    }
    const char* code = stage + 1;
    for (const AreaName& area : kAreas) {
        const size_t len = std::strlen(area.code);
        if (std::strncmp(code, area.code, len) == 0
            && (len == 2 || code[len] == '\0')) {
            return area.name;
        }
    }
    return nullptr;
}

const char* exact_area_name(const char* code, std::span<const AreaName> areas)
{
    for (const AreaName& area : areas) {
        if (std::strcmp(code, area.code) == 0) {
            return area.name;
        }
    }
    return nullptr;
}

const char* mgs2_area_name(const char* code)
{
    static constexpr AreaName kAreas[] = {
        {"w00a", "Aft Deck"}, {"w00b", "Aft Deck (Olga)"},
        {"w00c", "Navigational Deck"}, {"w01a", "Deck-A, Crew's Quarters"},
        {"w01b", "Deck-A, Crew's Quarters, Starboard"},
        {"w01c", "Deck-C, Crew's Quarters"}, {"w01d", "Deck-D, Crew's Quarters"},
        {"w01e", "Deck-E, Bridge"}, {"w01f", "Deck-A, Crew's Lounge"},
        {"w02a", "Engine Room"}, {"w03a", "Deck 2, Port"},
        {"w03b", "Deck 2, Starboard"}, {"w04a", "Hold No. 1"},
        {"w04b", "Hold No. 2"}, {"w04c", "Hold No. 3"},
        {"w11a", "Strut A, Deep Sea Dock"}, {"w11b", "Strut A, Deep Sea Dock"},
        {"w11c", "Strut A, Deep Sea Dock"}, {"w12a", "Strut A, Roof"},
        {"w12b", "Strut A, Pump Room"}, {"w12c", "Strut A, Roof"},
        {"w13a", "AB Connecting Bridge"}, {"w13b", "AB Connecting Bridge"},
        {"w14a", "Strut B, Transformer Room"}, {"w15a", "BC Connecting Bridge"},
        {"w15b", "BC Connecting Bridge"}, {"w16a", "Strut C, Dining Hall"},
        {"w16b", "Strut C, Dining Hall"}, {"w17a", "CD Connecting Bridge"},
        {"w18a", "Strut D, Sediment Pool"}, {"w19a", "DE Connecting Bridge"},
        {"w20a", "Strut E, Parcel Room"}, {"w20b", "Strut E, Heliport"},
        {"w20c", "Strut E, Heliport"}, {"w20d", "Strut E, Heliport"},
        {"w21a", "EF Connecting Bridge"}, {"w21b", "EF Connecting Bridge"},
        {"w22a", "Strut F, Warehouse"}, {"w23a", "FA Connecting Bridge"},
        {"w23b", "FA Connecting Bridge"}, {"w24a", "Shell 1 Core, 1F"},
        {"w24b", "Shell 1 Core, B1"}, {"w24c", "Shell 1 Core, B1 Hall"},
        {"w24d", "Shell 1 Core, B2"}, {"w24e", "Shell 1 Core"},
        {"w25a", "Shell 1-2 Connecting Bridge"},
        {"w25b", "Shell 1-2 Connecting Bridge"}, {"w25c", "Strut L Perimeter"},
        {"w25d", "KL Connecting Bridge"}, {"w28a", "Sewage Treatment Facility"},
        {"w31a", "Shell 2 Core, 1F"}, {"w31b", "Shell 2 Core, B1"},
        {"w31c", "Shell 2 Core, B1 Filtration Chamber"},
        {"w31d", "Shell 2 Core, 1F"}, {"w31f", "Shell 2 Core"},
        {"w32a", "Oil Fence"}, {"w32b", "Oil Fence"},
        {"w41a", "Arsenal Gear, Stomach"}, {"w42a", "Arsenal Gear, Jejunum"},
        {"w43a", "Arsenal Gear, Ascending Colon"}, {"w44a", "Arsenal Gear, Ileum"},
        {"w45a", "Arsenal Gear, Sigmoid Colon"}, {"w46a", "Arsenal Gear, Rectum"},
        {"w51a", "Arsenal Gear"}, {"w61a", "Federal Hall"},
    };
    return exact_area_name(code, kAreas);
}

const char* mgs2_area_state(const GameStats& stats)
{
    if (stats.mgs2_p_story == 0xFFFF || stats.mgs2_p_story > 600) return nullptr;
    for (const mgs2::DogTagAreaState& state : mgs2::kDogTagAreaStates) {
        if (std::strcmp(stats.area_code, state.area) == 0) {
            return stats.mgs2_p_story >= state.story ? state.after : state.before;
        }
    }
    struct FlagState {
        const char* area;
        int flag;
        const char* before;
        const char* after;
    };
    static constexpr FlagState kFlagStates[] = {
        {"w02a", 1, "before door repair", "after door repair"},
        {"w24d", 2, "before exercise soldier", "after exercise soldier"},
        {"w01b", 3, "before shadow", "after shadow"},
        {"w00c", 4, "before right door", "after right door"},
        {"w45a", 6, "before Tengu", "after Tengu"},
    };
    for (const FlagState& state : kFlagStates) {
        if (std::strcmp(stats.area_code, state.area) == 0) {
            const bool set = (stats.mgs2_dog_tag_flags & (uint32_t{1} << state.flag)) != 0;
            return set ? state.after : state.before;
        }
    }
    return nullptr;
}

const char* mgs3_area_name(const char* code)
{
    static constexpr AreaName kAreas[] = {
        {"v000a", "Dremuchij South (before backpack)"},
        {"v001a", "Dremuchij South (VM)"}, {"v003a", "Dremuchij Swampland (VM)"},
        {"v004a", "Dremuchij North (VM)"}, {"v005a", "Dolinovodno (VM)"},
        {"v006a", "Rassvet (before Ocelot)"}, {"v006b", "Rassvet (after Ocelot)"},
        {"v007a", "Dolinovodno Riverbank"},
        {"s001a", "Dremuchij South"}, {"s002a", "Dremuchij East"},
        {"s003a", "Dremuchij Swampland"}, {"s004a", "Dremuchij North"},
        {"s005a", "Dolinovodno"}, {"s006a", "Rassvet (before EVA)"},
        {"s006b", "Rassvet"}, {"s012a", "Chyornyj Prud"},
        {"s021a", "Bolshaya Past South"}, {"s022a", "Bolshaya Past Base"},
        {"s023a", "Bolshaya Past Crevice"},
        {"s031a", "Chyornaya Peschera Cave Branch"},
        {"s032a", "Chyornaya Peschera Cave"},
        {"s032b", "Chyornaya Peschera Cave (The Pain)"},
        {"s033a", "Chyornaya Peschera Cave Entrance"},
        {"s041a", "Ponizovje South"}, {"s042a", "Ponizovje West"},
        {"s043a", "Ponizovje Warehouse Exterior"},
        {"s044a", "Ponizovje Warehouse"}, {"s045a", "Svyatogornyj South"},
        {"s051a", "Graniny Gorki South"}, {"s051b", "Graniny Gorki South (The Fear)"},
        {"s052a", "Graniny Gorki Lab Exterior: Outside Walls"},
        {"s052b", "Graniny Gorki Lab Exterior: Inside Walls"},
        {"s053a", "Graniny Gorki Lab 1F/2F"},
        {"s055a", "Graniny Gorki Lab B1 East"},
        {"s056a", "Graniny Gorki Lab B1 West"},
        {"s061a", "Svyatogornyj West"}, {"s062a", "Svyatogornyj East"},
        {"s063a", "Sokrovenno South (The End)"},
        {"s063b", "Sokrovenno South (Ocelot Unit)"},
        {"s064a", "Sokrovenno West (The End)"},
        {"s064b", "Sokrovenno West (Ocelot Unit)"},
        {"s065a", "Sokrovenno North (The End)"},
        {"s065b", "Sokrovenno North (Ocelot Unit)"},
        {"s066a", "Krasnogorje Tunnel"}, {"s071a", "Krasnogorje Mountain Base"},
        {"s072a", "Krasnogorje Mountainside"}, {"s072b", "Krasnogorje Mountainside"},
        {"s073a", "Krasnogorje Mountaintop (before EVA)"},
        {"s073b", "Krasnogorje Mountaintop"},
        {"s074a", "Krasnogorje Mountaintop Ruins"},
        {"s075a", "Krasnogorje Mountaintop: Behind Ruins"},
        {"s081a", "Groznyj Grad Underground Tunnel"},
        {"s091a", "Groznyj Grad Southwest (before torture)"},
        {"s091b", "Groznyj Grad Southwest (prison escape)"},
        {"s091c", "Groznyj Grad Southwest"},
        {"s092a", "Groznyj Grad Northwest (before torture)"},
        {"s092b", "Groznyj Grad Northwest (prison escape)"},
        {"s092c", "Groznyj Grad Northwest"},
        {"s093a", "Groznyj Grad Northeast (before torture)"},
        {"s093b", "Groznyj Grad Northeast (prison escape)"},
        {"s093c", "Groznyj Grad Northeast"},
        {"s094a", "Groznyj Grad Southeast (before torture)"},
        {"s094b", "Groznyj Grad Southeast (prison escape)"},
        {"s094c", "Groznyj Grad Southeast"},
        {"s101a", "Weapons Lab East Wing (with Raikov)"},
        {"s101b", "Weapons Lab East Wing"},
        {"s111a", "Weapons Lab West Wing Corridor"},
        {"s112a", "Groznyj Grad Torture Room"}, {"s113a", "Groznyj Grad Sewers"},
        {"s121a", "Weapons Lab Main Wing"},
        {"s122a", "Weapons Lab Main Wing B1 (Volgin)"},
        {"s141a", "The Sorrow's River"}, {"s151a", "Tikhogornyj"},
        {"s152a", "Tikhogornyj: Behind Waterfall"}, {"s161a", "Groznyj Grad"},
        {"s162a", "Groznyj Grad Runway South"}, {"s163a", "Groznyj Grad Runway"},
        {"s163b", "Groznyj Grad Runway (Shagohod)"},
        {"s171a", "Groznyj Grad Rail Bridge"},
        {"s171b", "Groznyj Grad Rail Bridge (Shagohod)"},
        {"s181a", "Groznyj Grad Rail Bridge North"},
        {"s182a", "Lazorevo South"}, {"s183a", "Lazorevo North"},
        {"s191a", "Zaozyorje West (before patrol)"}, {"s191b", "Zaozyorje West"},
        {"s192a", "Zaozyorje East"}, {"s201a", "Rokovoj Bereg"},
        {"s211a", "WIG Interior"},
    };
    return exact_area_name(code, kAreas);
}

const char* mgs4_area_name(const char* code)
{
    static constexpr AreaName kAreas[] = {
        {"s00a00l", "Prologue Cemetery"},
        {"s00a10l", "Ending Cemetery"},
        {"s01a00l", "Middle East Infiltration"},
        {"s01a05l", "Middle East Infiltration"},
        {"s01a10l", "Red Zone"},
        {"s01a20l", "Militia Safehouse"},
        {"s01a30l", "Urban Ruins"},
        {"s01a40l", "Advent Palace"},
        {"s01a50l", "Crescent Meridian"},
        {"s01a55l", "Crescent Meridian"},
        {"s01a57l", "Millennium Park"},
        {"s01a60l", "Liquid's Encampment"},
        {"s02a10l", "Cove Valley Village"},
        {"s02a20l", "Power Station"},
        {"s02a25l", "Power Station"},
        {"s02a30l", "Confinement Facility"},
        {"s02a40l", "Vista Mansion"},
        {"s02a50l", "Research Lab"},
        {"s02a60l", "Mountain Trail / Riverside"},
        {"s02a70l", "Vamp Ambush"},
        {"s02a73l", "Stryker Escape"},
        {"s02a75l", "Stryker Escape"},
        {"s02a78l", "Stryker Escape"},
        {"s02a80l", "High Woodlands Highway"},
        {"s02a85l", "Marketplace Entrance"},
        {"s02a90l", "Marketplace"},
        {"s02a95l", "Marketplace Plaza"},
        {"s03a00l", "Eastern Europe Station"},
        {"s03a10l", "Midtown: Resistance Tail"},
        {"s03a15l", "Midtown: Resistance Tail"},
        {"s03a16l", "Midtown: Canals"},
        {"s03a20l", "Midtown: Plaza"},
        {"s03a25l", "Midtown: North Sector"},
        {"s03a30l", "Church Courtyard"},
        {"s03a35l", "Motorcycle Chase"},
        {"s03a40l", "Motorcycle Chase"},
        {"s03a50l", "Raging Raven Ambush"},
        {"s03a60l", "Motorcycle Chase"},
        {"s03a65l", "Echo's Beacon"},
        {"s03a70l", "Echo's Beacon"},
        {"s03a90l", "Volta River"},
        {"s04a05l", "Metal Gear Solid Flashback"},
        {"s04a10l", "Snowfield / Heliport / Tank Hangar"},
        {"s04a20l", "Nuclear Warhead Storage Building"},
        {"s04a30l", "Snowfield / Communications Tower"},
        {"s04a40l", "Blast Furnace / Casting Facility"},
        {"s04a50l", "Underground Base"},
        {"s04a60l", "Underground Supply Tunnel"},
        {"s04a65l", "REX Escape"},
        {"s04a68l", "Port Area"},
        {"s04a70l", "Port Area: REX vs. RAY"},
        {"s04a75l", "Outer Haven Arrival"},
        {"s05a10l", "Ship Bow"},
        {"s05a20l", "Command Center / Missile Hangar"},
        {"s05a30l", "Microwave Corridor"},
        {"s05a40l", "GW"},
        {"s05a45l", "Liquid Ocelot: Prelude"},
        {"s05a50l", "Liquid Ocelot"},
        {"s05a55l", "Liquid Ocelot: Aftermath"},
        {"s10a10l", "Nomad Mission Briefing"},
        {"s10a20l", "Nomad: South America Briefing"},
        {"s10a30l", "Nomad: Eastern Europe Briefing"},
        {"s10a40l", "Nomad: Shadow Moses Briefing"},
        {"s20a00l", "USS Missouri"},
        {"s20a10l", "USS Missouri vs. Outer Haven"},
        {"s20a20l", "Campbell's Room"},
        {"s30a00l", "Wedding"},
        {"s30a10l", "Hospital"},
    };
    if (const char* area = exact_area_name(code, kAreas)) return area;
    if (std::strncmp(code, "r_sna", 5) == 0) return "Nomad Mission Briefing";
    if (std::strncmp(code, "s00", 3) == 0) return "Prologue";
    if (std::strncmp(code, "s01", 3) == 0) return "Middle East";
    if (std::strncmp(code, "s02", 3) == 0) return "South America";
    if (std::strncmp(code, "s03", 3) == 0) return "Eastern Europe";
    if (std::strncmp(code, "s04", 3) == 0) return "Shadow Moses";
    if (std::strncmp(code, "s05", 3) == 0) return "Outer Haven";
    if (std::strncmp(code, "s06", 3) == 0) return "Epilogue";
    return nullptr;
}

const char* mgspw_area_name(const char* code, int region)
{
    if (const char* name = mgspw::region_name(region)) return name;
    static constexpr AreaName kAreas[] = {
        {"w01s03a", "Puerto del Alba"}, {"w01s04a", "El Cenagal: Jungle"},
        {"my_outer", "Mother Base"}, {"my_outer_ap", "Mother Base"},
        {"my_outer_trade", "Mother Base - Trade"}, {"my_outer_tfr", "Mother Base"},
        {"ms_lobby", "Mission Preparation"}, {"result", "Mission Results"},
        {"vs_lobby", "Versus Lobby"}, {"vs_result", "Versus Results"},
        {"title", "Title Screen"}, {"r_title", "Title Screen"},
        {"ending_flow", "Ending"}, {"browser", "Browser"},
    };
    return exact_area_name(code, kAreas);
}

const char* area_name(Game game, const char* code)
{
    switch (game) {
    case Game::MGS1: return mgs1_area_name(code);
    case Game::MGS2: return mgs2_area_name(code);
    case Game::MGS3: return mgs3_area_name(code);
    case Game::MGS4: return mgs4_area_name(code);
    default: return nullptr;
    }
}

struct IdColors {
    ImVec4 green;
    ImVec4 yellow;
    ImVec4 red;
};

float ui_size(float value)
{
    return value * ImGui::GetStyle().FontScaleDpi;
}

// Requirement-row verdict bands, shared by the Peace Walker and generic
// panels: this far into a stay-under limit reads as near the limit, and
// this far toward a reach goal reads as getting close. They sit here, next
// to IdColors, because the headless harnesses compile this block.
constexpr double kNearLimitShare = 0.75;
constexpr double kCloseGoalShare = 0.5;

// Match results refreshed with the 10Hz stats snapshot. draw_panel renders
// every Present, but nothing re-matches between snapshots; only genuinely
// live values (the PW mission clock) are read per frame at their use site.
// Lives in this block for the same harness reason as the bands above.
struct EvalCache {
    std::optional<codename::Match> match;
    std::vector<codename::Match> matches;
    std::vector<codename::ReqStatus> reqs;
};

EvalCache g_eval;

IdColors id_colors(Game game)
{
    switch (game) {
    case Game::MG1:  return {{0.48f, 0.72f, 0.38f, 1}, {0.80f, 0.48f, 0.04f, 1}, {0.82f, 0.20f, 0.10f, 1}};
    case Game::MG2:  return {{0.38f, 0.72f, 0.52f, 1}, {0.82f, 0.66f, 0.20f, 1}, {0.85f, 0.28f, 0.28f, 1}};
    case Game::MGS1: return {{0.42f, 0.88f, 0.66f, 1}, {0.88f, 0.72f, 0.28f, 1}, {0.90f, 0.32f, 0.30f, 1}};
    case Game::MGS2: return {{0.42f, 0.82f, 0.52f, 1}, {0.92f, 0.70f, 0.24f, 1}, {0.76f, 0.19f, 0.11f, 1}};
    case Game::MGS3: return {{0.55f, 0.78f, 0.42f, 1}, {0.88f, 0.72f, 0.28f, 1}, {0.82f, 0.32f, 0.24f, 1}};
    case Game::MGS4: return {{0.48f, 0.80f, 0.40f, 1}, {0.90f, 0.72f, 0.28f, 1}, {0.88f, 0.30f, 0.24f, 1}};
    // The red is the menu red the theme is built on; green and amber only have
    // to carry a verdict against near-white text on black.
    case Game::MGSPW: return {{0.30f, 0.86f, 0.40f, 1}, {0.96f, 0.78f, 0.24f, 1}, {0.95f, 0.11f, 0.14f, 1}};
    // Straight off the Ghost Babel title menu: NEW GAME's green and CONTINUE's
    // red. That screen has no yellow, so the middle band is inferred at the
    // same saturation.
    case Game::Babel: return {{0.00f, 0.63f, 0.36f, 1}, {0.94f, 0.78f, 0.00f, 1}, {0.97f, 0.00f, 0.00f, 1}};
    }
    return {{0.42f, 0.90f, 0.45f, 1}, {1.0f, 0.82f, 0.25f, 1}, {0.95f, 0.35f, 0.35f, 1}};
}

// Dimmed text for a value the probe has not resolved, or an entry not yet
// earned. Each game theme sets its own TextDisabled.
ImVec4 unset_color()
{
    return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
}

// Thousands as "50k" / "12.8k". The PW grade gates run to six figures, which
// is more digits than the columns they sit in can carry.
void format_thousands(double value, char* buf, size_t len)
{
    if (value < 1000) {
        snprintf(buf, len, "%.0f", value);
    } else if (std::fmod(value, 1000.0) == 0.0 || value >= 100000) {
        snprintf(buf, len, "%.0fk", value / 1000.0);
    } else {
        snprintf(buf, len, "%.1fk", value / 1000.0);
    }
}

// Up/Down move a scrolling panel by eight lines a press.
void apply_scroll(int scroll)
{
    if (scroll != 0) {
        ImGui::SetScrollY(ImGui::GetScrollY()
                          + scroll * ImGui::GetTextLineHeightWithSpacing() * 8);
    }
}

void align_value(const char* text)
{
    ImGui::SetCursorPosX(ImGui::GetCursorPosX()
        + std::fmax(0.0f, ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(text).x));
}

void stat_row(const char* key, const char* value)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(key);
    ImGui::TableNextColumn();
    if (g_game == Game::MGSPW) align_value(value);
    ImGui::TextUnformatted(value);
}

// A row whose value the probe has not resolved.
void unset_row(const char* key)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(key);
    ImGui::TableNextColumn();
    if (g_game == Game::MGSPW) align_value("n/a");
    ImGui::TextDisabled("n/a");
}

// Same row, dimmed: context the panel shows but does not rank on.
void dim_row(const char* key, const char* value)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextDisabled("%s", key);
    ImGui::TableNextColumn();
    if (g_game == Game::MGSPW) align_value(value);
    ImGui::TextDisabled("%s", value);
}

// The special-items row reads as the list of items actually used. Each game
// has its own set, one per bit of special_items_mask.
std::span<const char* const> special_item_names(Game game)
{
    static constexpr const char* kMgs2[] = {
        "Infinity Bandana", "Infinity Wig", "O2 Wig", "Grip Wig", "Stealth Camo"};
    static constexpr const char* kMgs3[] = {
        "Stealth Camo", "Infinity Face Paint", "EZ Gun"};
    static constexpr const char* kMgs4[] = {"Infinity Bandana", "Stealth Camo"};
    switch (game) {
    case Game::MGS2: return kMgs2;
    case Game::MGS3: return kMgs3;
    case Game::MGS4: return kMgs4;
    default: return {};
    }
}

// Comma-joined names of the set bits, or NONE when none of them is set. Bits
// above the name list are ignored, which is what bounds the mask.
void join_flags(char* out, size_t len, uint16_t mask, std::span<const char* const> names)
{
    out[0] = '\0';
    for (size_t i = 0; i < names.size(); ++i) {
        if ((mask & (1u << i)) == 0) {
            continue;
        }
        const size_t used = std::strlen(out);
        snprintf(out + used, len - used, "%s%s", used ? ", " : "", names[i]);
    }
    if (out[0] == '\0') {
        snprintf(out, len, "NONE");
    }
}

bool begin_fitted_child(const char* id)
{
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, ui_size(360)));
    return ImGui::BeginChild(id, ImVec2(0, 0),
                             ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
}

void checklist(const char* id, const char* const* names, size_t count, uint64_t mask, int scroll)
{
    const ImVec4 done_color = id_colors(g_game).green;
    const ImVec4 todo_color = unset_color();
    if (begin_fitted_child(id)) {
        apply_scroll(scroll);
        for (size_t i = 0; i < count; ++i) {
            const bool done = (mask & (uint64_t{1} << i)) != 0;
            ImGui::TextColored(done ? done_color : todo_color,
                               "%s  %s", done ? "x" : "-", names[i]);
        }
    }
    ImGui::EndChild();
}

void draw_mgs4_feats(const GameStats& stats, int scroll)
{
    const auto& matches = g_eval.matches;
    const auto matched = [&](const char* name) {
        for (const auto& match : matches) {
            if (std::strcmp(match.name, name) == 0) return true;
        }
        return false;
    };
    if (begin_fitted_child("mgs4_feats")) {
        apply_scroll(scroll);
        if (ImGui::BeginTable("mgs4_feat_rows", 2,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("emblem", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("progress", ImGuiTableColumnFlags_WidthFixed, ui_size(180));
            const ImVec4 done_color = id_colors(g_game).green;
            const ImVec4 pending_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            const auto row = [&](const char* name, const char* value) {
                const bool done = matched(name);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(done ? done_color : pending_color, "%s", name);
                ImGui::TableNextColumn();
                ImGui::TextColored(done ? done_color : pending_color, "%s", value);
            };
            const auto count = [&](const char* name, int value, int target, const char* unit) {
                char text[64];
                snprintf(text, sizeof(text), "%d / %d %s", value, target, unit);
                row(name, text);
            };
            const auto time = [&](const char* name, double seconds, double target_seconds,
                                  const char* unit) {
                char current[16], target[16], text[64];
                format_time(seconds, current, sizeof(current));
                format_time(target_seconds, target, sizeof(target));
                snprintf(text, sizeof(text), "%s / %s %s", current, target, unit);
                row(name, text);
            };
            char text[96];
            // Goals come from shared table so panel cannot drift from rank rules.
            namespace goals = codename::mgs4_goals;
            count("BEAR", stats.cqc_chokes, goals::kBearChokes, "CQC chokes");
            count("EAGLE", stats.headshots, goals::kEagleHeadshots, "headshots");
            snprintf(text, sizeof(text), "knife %d / %d\nCQC %d / %d\nalerts %d / %d",
                      stats.knife_defeats, goals::kAssassinKnife,
                      stats.cqc_holds, goals::kAssassinCqcHolds,
                      stats.alerts, goals::kAssassinMaxAlerts);
            row("ASSASSIN", text);
            snprintf(text, sizeof(text), "%d kills", stats.kills);
            row("PIGEON", text);
            count("BLUE BIRD", stats.items_given, goals::kBlueBirdItems, "items given");
            count("HAWK", stats.praises, goals::kHawkPraises, "praises");
            count("LITTLE GRAY", stats.weapons_acquired, goals::kLittleGrayWeapons, "weapons");
            count("ANT", stats.body_searches, goals::kAntSearches, "searches");
            count("GIBBON", stats.hold_ups, goals::kGibbonHoldUps, "hold-ups");
            time("TORTOISE", stats.box_time_seconds, goals::kTortoiseBoxMinutes * 60, "box time");
            count("RABBIT", stats.magazine_pages, goals::kRabbitPages, "pages");
            count("BEE", stats.syringe_uses, goals::kBeeSyringeUses, "uses");
            time("GECKO", stats.wall_time_seconds, goals::kGeckoWallMinutes * 60, "wall time");
            count("SCARAB", stats.side_rolls, goals::kScarabSideRolls, "side rolls");
            count("FROG", stats.forward_rolls, goals::kFrogForwardRolls, "forward rolls");
            time("INCH WORM", stats.crawl_time_seconds, goals::kInchWormCrawlMinutes * 60, "crawl time");
            time("LOBSTER", stats.crouch_time_seconds, goals::kLobsterCrouchMinutes * 60, "crouch time");
            count("HYENA", stats.pickups, goals::kHyenaPickups, "pickups");
            count("HOG", stats.combat_highs, goals::kHogCombatHighs, "combat highs");
            count("PIG", stats.rations_used, goals::kPigRations, "recovery items");
            count("COW", stats.alerts, goals::kCowAlerts, "alerts");
            count("CROCODILE", stats.kills, goals::kCrocodileKills, "kills");
            time("GIANT PANDA", stats.play_time_seconds, goals::kGiantPandaHours * 60 * 60, "play time");
            char current[16], target[16], chicken[192];
            format_time(stats.play_time_seconds, current, sizeof(current));
            format_time(goals::kChickenHours * 60 * 60, target, sizeof(target));
            snprintf(chicken, sizeof(chicken),
                     "alerts %d / %d\nkills %d / %d\ncontinues %d / %d\nrecovery items %d / %d\nplay time %s / %s",
                     stats.alerts, goals::kChickenAlerts,
                     stats.kills, goals::kChickenKills,
                     stats.continues, goals::kChickenContinues,
                     stats.rations_used, goals::kChickenRecoveryItems, current, target);
            row("CHICKEN", chicken);
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
}

constexpr const char* kMgs3Captures[] = {
    "King Cobra", "Taiwanese Cobra", "Thai Cobra", "Coral Snake",
    "Milk Snake", "Green Tree Python", "Giant Anaconda", "Reticulated Python",
    "Snake Liquid", "Snake Solid", "Snake Solidus", "Indian Gavial",
    "Otton Frog", "Tree Frog", "Poison Dart Frog", "Rat",
    "European Rabbit", "Flying Squirrel", "Markhor", "Vampire Bat",
    "Hornet's Nest", "Emperor Scorpion", "Cobalt Blue Tarantula", "Parrot",
    "White-Rumped Vulture", "Red Avadavat", "Magpie", "Sund Whistling-Thrush",
    "Bigeye Trevally", "Maroon Shark", "Arowana", "Kenyan Mangrove Crab",
    "Russian Oyster Mushroom", "Ural Luminescent Mushroom", "Siberian Ink Cap",
    "Fly Agaric", "Russian Glowcap", "Spatsa", "Baikal Scaly Tooth",
    "Yabloko Moloko", "Russian False Mango", "Golova", "Vine Melon",
    "Instant Noodles", "Russian Ration", "Calorie Mate", "Hive of Pain Hornets",
    "Tsuchinoko",
};

constexpr const char* kMgs3Kerotans[] = {
    "01 Dremuchij South (VM)", "02 Dremuchij Swampland (VM)", "03 Dremuchij North (VM)",
    "04 Dolinovodno (VM)", "05 Rassvet (VM)", "06 Dremuchij South",
    "07 Dremuchij East", "08 Dremuchij Swampland", "09 Dremuchij North",
    "10 Dolinovodno", "11 Rassvet", "12 Chyornyj Prud",
    "13 Bolshaya Past South", "14 Bolshaya Past Base", "15 Bolshaya Past Crevice",
    "16 Chyornaya Peschera Cave Branch", "17 Chyornaya Peschera Cave", "18 Chyornaya Peschera Cave Entrance",
    "19 Ponizovje South", "20 Ponizovje West", "21 Ponizovje Warehouse Exterior",
    "22 Ponizovje Warehouse 1F", "23 Svyatogornyj South", "24 Graniny Gorki South",
    "25 Graniny Gorki Lab Exterior Perimeter", "26 Graniny Gorki Lab Exterior Yard", "27 Graniny Gorki Lab 1F",
    "28 Graniny Gorki Lab B1 East", "29 Graniny Gorki Lab B1 West", "30 Svyatogornyj West",
    "31 Svyatogornyj East", "32 Sokrovenno South", "33 Sokrovenno West",
    "34 Sokrovenno North", "35 Krasnogorje Tunnel", "36 Krasnogorje Mountain Base",
    "37 Krasnogorje Mountainside", "38 Krasnogorje Mountaintop", "39 Krasnogorje Mountaintop Ruins",
    "40 Krasnogorje Mountaintop Behind Ruins", "41 Groznyj Grad Underground Tunnel", "42 Groznyj Grad Southwest",
    "43 Groznyj Grad Northwest", "44 Groznyj Grad Northeast", "45 Groznyj Grad Southeast",
    "46 Weapons Lab East Wing 2F", "47 Weapons Lab West Wing 2F Corridor", "48 Groznyj Grad Holding Facility",
    "49 Weapons Lab Main Wing 1F", "50 Groznyj Grad B1F", "51 Tikhogornyj",
    "52 Tikhogornyj Behind Waterfall", "53 Groznyj Grad Escape", "54 Groznyj Grad Runway South",
    "55 Groznyj Grad Runway", "56 Groznyj Grad Runway After WIG", "57 Rail Bridge C3",
    "58 Rail Bridge Shagohod", "59 Rail Bridge North", "60 Lazorevo South",
    "61 Lazorevo North", "62 Zaozyorje West", "63 Zaozyorje East",
    "64 Rokovoj Bereg",
};

static_assert(std::size(kMgs3Captures) == 48);
static_assert(std::size(kMgs3Kerotans) == 64);

void draw_mgs2_dog_tags(const GameStats& stats, int scroll)
{
    const uint8_t difficulty = static_cast<uint8_t>(stats.difficulty);
    int available = 0;
    int collected = 0;
    for (const auto& tag : mgs2::kDogTags) {
        if (!mgs2::dog_tag_available(tag, stats.mission, difficulty)) continue;
        ++available;
        collected += mgs2::dog_tag_collected(stats.dog_tag_mask, tag.id);
    }

    ImGui::Text("%d / %d", collected, available);
    if (collected != stats.dog_tags) {
        ImGui::SameLine();
        ImGui::TextDisabled("(%d total flags)", stats.dog_tags);
    }
    {
        const char* variant = stats.mgs2_dog_tags_2002 ? "2002" : "2001";
        const float width = ImGui::CalcTextSize(variant).x;
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - width);
        ImGui::TextUnformatted(variant);
    }
    if (begin_fitted_child("mgs2_dog_tags")) {
        apply_scroll(scroll);
        // ponytail: fixed 34 x 394 scan; index only if roster becomes dynamic.
        for (const char* area_code : mgs2::kDogTagAreas) {
            bool header = false;
            for (const auto& tag : mgs2::kDogTags) {
                if (std::strcmp(tag.area, area_code) != 0
                    || !mgs2::dog_tag_available(tag, stats.mission, difficulty)) {
                    continue;
                }
                if (!header) {
                    const char* area = area_name(Game::MGS2, area_code);
                    ImGui::SeparatorText(area ? area : area_code);
                    header = true;
                }
                const bool done = mgs2::dog_tag_collected(stats.dog_tag_mask, tag.id);
                const bool state_known = stats.mgs2_p_story != 0xFFFF
                    && stats.mgs2_p_story <= 600;
                const mgs2::DogTagGate* gate =
                    state_known ? mgs2::dog_tag_gate(tag.id) : nullptr;
                const bool gate_ok = !gate
                    || mgs2::dog_tag_gate_available(*gate, stats.mgs2_p_story,
                                                    stats.mgs2_dog_tag_flags);
                ImVec4 color = done ? id_colors(g_game).green : unset_color();
                if (!done && gate && !gate_ok) {
                    color = gate->kind == mgs2::DogTagGateKind::Until
                        ? id_colors(g_game).red : unset_color();
                }
                char suffix[48] = {};
                if (gate) {
                    std::snprintf(suffix, sizeof(suffix), "  [%s %s]",
                                  gate->kind == mgs2::DogTagGateKind::After ? "after"
                                                                             : "until",
                                  gate->trigger);
                }
                char text[160];
                std::snprintf(text, sizeof(text), "%s  %s%s", done ? "x" : "-",
                              mgs2::dog_tag_name(tag, stats.mgs2_dog_tags_2002), suffix);
                ImGui::TextColored(color, "%s", text);
                if (tag.bluff) {
                    ImVec2 pos = ImGui::GetItemRectMin();
                    pos.x += 1.0f;
                    ImGui::GetWindowDrawList()->AddText(pos, ImGui::GetColorU32(color), text);
                }
            }
        }
    }
    ImGui::EndChild();
}

int mgs3_area_kerotan(const char* code)
{
    static constexpr AreaName kAreas[] = {
        {"v001a", "0"},
        {"v003a", "1"},
        {"v004a", "2"},
        {"v005a", "3"},
        {"v006a", "4"},
        {"v006b", "4"},
        {"s001a", "5"},
        {"s002a", "6"},
        {"s003a", "7"},
        {"s004a", "8"},
        {"s005a", "9"},
        {"s006a", "10"},
        {"s006b", "10"},
        {"s012a", "11"},
        {"s021a", "12"},
        {"s022a", "13"},
        {"s023a", "14"},
        {"s031a", "15"},
        {"s032a", "16"},
        {"s032b", "16"},
        {"s033a", "17"},
        {"s041a", "18"},
        {"s042a", "19"},
        {"s043a", "20"},
        {"s044a", "21"},
        {"s045a", "22"},
        {"s051a", "23"},
        {"s051b", "23"},
        {"s052a", "24"},
        {"s052b", "25"},
        {"s053a", "26"},
        {"s055a", "27"},
        {"s056a", "28"},
        {"s061a", "29"},
        {"s062a", "30"},
        {"s063a", "31"},
        {"s063b", "31"},
        {"s064a", "32"},
        {"s064b", "32"},
        {"s065a", "33"},
        {"s065b", "33"},
        {"s066a", "34"},
        {"s071a", "35"},
        {"s072a", "36"},
        {"s072b", "36"},
        {"s073a", "37"},
        {"s073b", "37"},
        {"s074a", "38"},
        {"s075a", "39"},
        {"s081a", "40"},
        {"s091a", "41"},
        {"s091b", "41"},
        {"s091c", "41"},
        {"s092a", "42"},
        {"s092b", "42"},
        {"s092c", "42"},
        {"s093a", "43"},
        {"s093b", "43"},
        {"s093c", "43"},
        {"s094a", "44"},
        {"s094b", "44"},
        {"s094c", "44"},
        {"s101a", "45"},
        {"s101b", "45"},
        {"s111a", "46"},
        {"s112a", "47"},
        {"s121a", "48"},
        {"s122a", "49"},
        {"s151a", "50"},
        {"s152a", "51"},
        {"s161a", "52"},
        {"s162a", "53"},
        {"s163a", "54"},
        {"s163b", "55"},
        {"s171a", "56"},
        {"s171b", "57"},
        {"s181a", "58"},
        {"s182a", "59"},
        {"s183a", "60"},
        {"s191a", "61"},
        {"s191b", "61"},
        {"s192a", "62"},
        {"s201a", "63"},
    };
    for (const AreaName& area : kAreas) {
        if (std::strncmp(code, area.code, 5) == 0
            && (code[5] == '\0' || code[5] == '_')) {
            return std::atoi(area.name);
        }
    }
    return -1;
}

using EvaluateFn = std::optional<codename::Match> (*)(const GameStats&);
using RequirementsFn = std::vector<codename::ReqStatus> (*)(const GameStats&);

constexpr EvaluateFn kEvaluateFns[] = {
    [static_cast<int>(Game::MG1)] = codename::evaluate_mg1,
    [static_cast<int>(Game::MG2)] = codename::evaluate_mg2,
    [static_cast<int>(Game::MGS1)] = codename::evaluate_mgs1,
    [static_cast<int>(Game::MGS2)] = codename::evaluate_mgs2,
    [static_cast<int>(Game::MGS3)] = codename::evaluate_mgs3,
    [static_cast<int>(Game::MGS4)] = codename::evaluate_mgs4,
    [static_cast<int>(Game::MGSPW)] = codename::evaluate_mgspw,
    [static_cast<int>(Game::Babel)] = codename::evaluate_babel,
};

constexpr RequirementsFn kRequirementsFns[] = {
    [static_cast<int>(Game::MG1)] = codename::elite_requirements_mg1,
    [static_cast<int>(Game::MG2)] = codename::elite_requirements_mg2,
    [static_cast<int>(Game::MGS1)] = codename::elite_requirements_mgs1,
    [static_cast<int>(Game::MGS2)] = codename::elite_requirements_mgs2,
    [static_cast<int>(Game::MGS3)] = codename::elite_requirements_mgs3,
    [static_cast<int>(Game::MGS4)] = codename::elite_requirements_mgs4,
    [static_cast<int>(Game::MGSPW)] = codename::elite_requirements_mgspw,
    [static_cast<int>(Game::Babel)] = codename::elite_requirements_babel,
};

static_assert(std::size(kEvaluateFns) == 8);
static_assert(std::size(kRequirementsFns) == 8);

// Live sortie clock and tally.
void draw_mgspw_run(const GameStats& stats)
{
    const auto [id_green, id_yellow, id_red] = id_colors(Game::MGSPW);
    // Poll 300 Hz clock each frame for smooth milliseconds.
    const bool results = std::strcmp(stats.pw_stage, "result") == 0;
    uint32_t ticks = results && stats.pw_result_time
        ? stats.pw_result_time : stats.pw_mission_play;
    if (!results && g_clock_fn) {
        g_clock_fn(ticks);
    }
    const unsigned long long mission_ms = static_cast<unsigned long long>(ticks) * 1000ULL / 300ULL;
    const unsigned long long best_ms =
        static_cast<unsigned long long>(stats.pw_cur_best) * 1000ULL / 300ULL;
    ImGui::SetWindowFontScale(2.0f);
    ImGui::Text("%llu:%02llu.%03llu", mission_ms / 60000, (mission_ms / 1000) % 60,
                mission_ms % 1000);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::Spacing();

    char buf[64];
    if (stats.pw_mission_id > 0) {
        const char* rank_names[] = {"S", "A", "B", "C"};
        const char* rank = stats.pw_cur_rank >= 0 && stats.pw_cur_rank < 4
            ? rank_names[stats.pw_cur_rank] : "-";
        if (stats.pw_cur_best) {
            // Rank and time are independent bests.
            ImGui::TextDisabled("Mission %d | Best rank %s", stats.pw_mission_id, rank);
            ImGui::TextDisabled("Best time %llu:%02llu.%03llu", best_ms / 60000, (best_ms / 1000) % 60,
                                best_ms % 1000);
            const double delta = (double(mission_ms) - double(best_ms)) / 1000.0;
            const double size = delta < 0 ? -delta : delta;
            char gap[24];
            if (size < 60.0) {
                snprintf(gap, sizeof(gap), "%c%.1fs", delta < 0 ? '-' : '+', size);
            } else {
                snprintf(gap, sizeof(gap), "%c%d:%04.1f", delta < 0 ? '-' : '+',
                         int(size) / 60, std::fmod(size, 60.0));
            }
            ImGui::SameLine();
            ImGui::TextColored(delta <= 0 ? id_green : id_red, "%s", gap);
        } else {
            ImGui::TextDisabled("Mission %d | No time recorded", stats.pw_mission_id);
        }
    }
    // Some segment deltas settle at results or lobby exit.
    if (ImGui::BeginTable("pw_current", 2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("this run", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("mission", ImGuiTableColumnFlags_WidthFixed, ui_size(84));
        ImGui::TableHeadersRow();
        // Prefer native mission tally; fall back to client-side delta.
        const auto run_stat = [&](const char* k, int mission_value, int seg) {
            if (mission_value >= 0) {
                snprintf(buf, sizeof(buf), "%d", mission_value);
            } else {
                snprintf(buf, sizeof(buf), "%+d (area)", seg);
            }
            stat_row(k, buf);
        };
        // Non-zero kills or alerts forfeit clean-clear insignias.
        const auto clean_row = [&](const char* k, int mission_value, int seg) {
            if (mission_value < 0) {
                run_stat(k, mission_value, seg);
                return;
            }
            snprintf(buf, sizeof(buf), "%d", mission_value);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(k);
            ImGui::TableNextColumn();
            align_value(buf);
            ImGui::TextColored(mission_value ? id_red : id_green, "%s", buf);
        };
        clean_row("kills", stats.pw_m_kills, stats.seg_kills);
        if (stats.pw_m_alerts < 0) unset_row("alerts");
        else clean_row("alerts", stats.pw_m_alerts, 0);
        run_stat("headshots", stats.pw_m_headshots, stats.seg_headshots);
        if (stats.pw_m_holdups < 0) unset_row("hold-ups");
        else run_stat("hold-ups", stats.pw_m_holdups, 0);
        if (stats.pw_m_cqc_uses < 0) unset_row("CQC uses");
        else run_stat("CQC uses", stats.pw_m_cqc_uses, 0);
        if (stats.pw_m_stun_rod_takedowns < 0) unset_row("stun rod KOs");
        else run_stat("stun rod KOs", stats.pw_m_stun_rod_takedowns, 0);
        run_stat("tranq", stats.pw_m_tranq, stats.seg_tranq);
        const bool mission_heroism = stats.pw_m_heroism >= 0;
        snprintf(buf, sizeof(buf), "%+d%s",
                 mission_heroism ? stats.pw_m_heroism : stats.seg_heroism,
                 mission_heroism ? "" : " (area)");
        stat_row("heroism", buf);
        // Full health is the deployed soldier's own maximum, not a constant.
        if (stats.pw_player_max_hp > 0) {
            constexpr int kHpGreenPct = 50;
            constexpr int kHpYellowPct = 25;
            const int hp_pct = stats.pw_player_hp * 100 / stats.pw_player_max_hp;
            snprintf(buf, sizeof(buf), "%d%%", hp_pct);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("HP");
            ImGui::TableNextColumn();
            align_value(buf);
            ImGui::TextColored(hp_pct >= kHpGreenPct ? id_green : hp_pct >= kHpYellowPct ? id_yellow : id_red,
                               "%s", buf);
        } else {
            unset_row("HP");
        }
        ImGui::EndTable();
    }
}

void draw_mgspw_summary(const GameStats& stats, int)
{
    const char* stage = stats.pw_stage[0] ? stats.pw_stage : "-";
    const char* name = mgspw_area_name(stage, stats.pw_region_id);
    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);
    if (name) {
        ImGui::TextDisabled("%s", name);
    } else {
        ImGui::TextDisabled("%s", stage);
    }
    ImGui::PopTextWrapPos();

    if (stats.pw_in_mission) {
        draw_mgspw_run(stats);
    } else {
        // Later menus may still hold last run's tally. Hide it once results ends.
        ImGui::TextDisabled("No mission running");
        ImGui::Separator();
    }

    ImGui::Spacing();
    const auto match = codename::evaluate_mgspw(stats);
    ImGui::TextDisabled("Projected codename: %s", match ? match->name : "-");
}

void draw_mgspw_global(const GameStats& stats, int scroll)
{
    begin_fitted_child("pw_career_scroll");
    apply_scroll(scroll);
    char buf[64];
    const auto count = [&](const char* label, int value) {
        if (value < 0) {
            unset_row(label);
        } else {
            format_count(value, buf, sizeof(buf));
            stat_row(label, buf);
        }
    };
    if (ImGui::BeginTable("pw_totals", 2, ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("outcome", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("total", ImGuiTableColumnFlags_WidthFixed, ui_size(90));
        count("kills", stats.pw_kills);
        count("non-lethal takedowns", stats.pw_codename_axes_ok
              ? codename::pw_axes(stats).nonlethal : -1);
        ImGui::EndTable();
    }
    ImGui::Spacing();
    if (ImGui::BeginTable("pw_weapons", 3,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("takedowns", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("lethal", ImGuiTableColumnFlags_WidthFixed, ui_size(75));
        ImGui::TableSetupColumn("non-lethal", ImGuiTableColumnFlags_WidthFixed, ui_size(85));
        ImGui::TableHeadersRow();
        const auto weapon = [&](const char* label, int slot, int lethal, int nonlethal) {
            if (stats.pw_codename_axes_ok) {
                lethal = stats.pw_codename_axes[0][slot];
                nonlethal = stats.pw_codename_axes[1][slot]
                    + stats.pw_codename_axes[2][slot] + stats.pw_codename_axes[3][slot];
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label);
            for (int value : {lethal, nonlethal}) {
                ImGui::TableNextColumn();
                if (value < 0) snprintf(buf, sizeof(buf), "n/a");
                else format_count(value, buf, sizeof(buf));
                align_value(buf);
                if (value < 0) ImGui::TextDisabled("%s", buf);
                else ImGui::TextUnformatted(buf);
            }
        };
        weapon("handguns", 2, stats.pw_pistol_lethal, stats.pw_pistol_takedowns);
        weapon("assault rifles", 3, stats.pw_ar_takedowns, -1);
        weapon("sniper rifles", 4, stats.pw_sniper_takedowns, stats.pw_sniper_nonlethal);
        weapon("machine guns", 5, stats.pw_lmg_takedowns, -1);
        weapon("submachine guns", 6, -1, -1);
        weapon("shotguns", 7, stats.pw_shotgun_takedowns, -1);
        weapon("CQC", 0, -1, stats.pw_cqc_takedowns);
        weapon("stun rod", 1, -1, stats.pw_stun_rod_takedowns);
        weapon("grenades", 9, stats.pw_grenade_takedowns, -1);
        weapon("rockets", 8, stats.pw_rocket_takedowns, -1);
        weapon("placed explosives", 11, stats.pw_placed_takedowns, -1);
        ImGui::EndTable();
    }
    ImGui::Spacing();
    if (ImGui::BeginTable("pw_global", 2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("career", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("total", ImGuiTableColumnFlags_WidthFixed, ui_size(126));
        ImGui::TableHeadersRow();
        format_time(stats.pw_total_play, buf, sizeof(buf));
        stat_row("play time", buf);
        char total[16], delta[16];
        format_count(stats.pw_heroism, total, sizeof(total));
        format_count(stats.pw_heroism_delta, delta, sizeof(delta));
        snprintf(buf, sizeof(buf), "%s (%s%s)", total,
                 stats.pw_heroism_delta >= 0 ? "+" : "", delta);
        stat_row("heroism", buf);
        format_count(stats.pw_gmp, buf, sizeof(buf));
        stat_row("GMP", buf);
        count("camaraderie", stats.pw_camaraderie);
        count("clears (with replays)", stats.pw_clears);
        count("unique missions", stats.pw_unique_cleared);
        count("S-ranked missions", stats.pw_s_missions);
        count("headshots", stats.pw_headshots);
        count("alerts", stats.pw_alerts);
        count("unseen kills", stats.pw_stealth_kills);
        count("enemy Fultons", stats.pw_fulton_recoveries);
        count("prisoners extracted", stats.pw_prisoner_extractions);
        count("hold-ups", stats.pw_holdups);
        count("CQC uses", stats.pw_cqc_uses);
        count("no-kill clears", stats.pw_nokill_clears);
        count("no-alert clears", stats.pw_noalert_clears);
        count("no-recovery-item clears", stats.pw_noitem_clears);
        if (stats.pw_damage_taken >= 0) {
            snprintf(buf, sizeof(buf), "%d (%.1f bars)", stats.pw_damage_taken,
                     stats.pw_damage_taken / 8000.0);
            stat_row("damage taken", buf);
        } else {
            unset_row("damage taken");
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::EndChild();
}

void draw_mgspw_insignia(const GameStats& stats, int)
{
    // Each insignia family has three consecutive rising thresholds.
    static constexpr struct {
        const char* label;
        int first_id;
    } kFamilies[] = {
        {"no-alert clears", 1},  {"no-kill clears", 4}, {"hold-ups", 7},
        {"no-item clears", 10},  {"headshots", 16},     {"Fulton recoveries", 44},
    };
    const auto [id_green, id_yellow, id_red] = id_colors(Game::MGSPW);
    if (stats.pw_insignias >= 0) {
        ImGui::Text("%d / 110 insignias earned", stats.pw_insignias);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram,
                              ImVec4(id_green.x, id_green.y, id_green.z, 0.55f));
        ImGui::ProgressBar(stats.pw_insignias / 110.0f, ImVec2(-FLT_MIN, ui_size(6)), "");
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    bool unresolved = false;
    if (ImGui::BeginTable("pw_insignia_stats", 2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("counter", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("next", ImGuiTableColumnFlags_WidthFixed, ui_size(100));
        ImGui::TableHeadersRow();
        const ImVec4 pending = unset_color();
        for (const auto& family : kFamilies) {
            const int have = codename::pw_insignia_progress(family.first_id, stats);
            // Show first unbeaten C/B/A threshold.
            int target = -1;
            const char* tier_name = "";
            for (int tier = 0; tier < 3 && target < 0; ++tier) {
                const int over = codename::pw_insignia(family.first_id + tier).over;
                if (have <= over) {
                    // Grant test is strict.
                    target = over + 1;
                    static constexpr const char* kTiers[] = {"C", "B", "A"};
                    tier_name = kTiers[tier];
                }
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(family.label);
            ImGui::TableNextColumn();
            if (have < 0) {
                unresolved = true;
                ImGui::TextColored(pending, "-");
            } else if (target < 0) {
                ImGui::TextColored(id_green, "%d  done", have);
            } else {
                ImGui::TextColored(have * 4 >= target * 3 ? id_yellow : pending, "%d / %d  %s",
                                   have, target, tier_name);
            }
        }
        ImGui::EndTable();
    }
    if (unresolved) {
        ImGui::TextDisabled("- = counter not resolved yet");
    }
}

void draw_mgspw_codenames(const GameStats& stats, int)
{
    // Weapon distribution decides all-weapons titles.
    const auto [id_green, id_yellow, id_red] = id_colors(Game::MGSPW);
    const codename::PwAxes axes = codename::pw_axes(stats);
    const ImVec4 pending = unset_color();
    const auto match = codename::evaluate_mgspw(stats);
    const ImVec4 title_color = !match ? pending
        : std::strcmp(match->name, "FOXHOUND") == 0 || std::strcmp(match->name, "FOX") == 0
            ? id_green
            : ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImGui::TextColored(title_color, "%s", match ? match->name : "---");
    ImGui::SameLine();
    ImGui::TextDisabled("projected");
    if (stats.pw_codename_state_ok) {
        int owned = 0;
        for (int id = 1; id <= 24; ++id) {
            owned += stats.pw_codename_state[id] & 1;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("| %d / 24 earned", owned);
    }
    if (!axes.native) ImGui::TextDisabled("Estimated from available counters");
    char buf[64];
    ImGui::Spacing();
    if (ImGui::BeginTable("pw_reqs", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("FOX / FOXHOUND", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("have / need", ImGuiTableColumnFlags_WidthFixed, ui_size(100));
        ImGui::TableHeadersRow();
        for (const codename::ReqStatus& r : g_eval.reqs) {
            const char* pct =
                static_cast<codename::ReqFmt>(r.fmt) == codename::ReqFmt::Percent ? "%" : "";
            if (r.limit == 0) {
                snprintf(buf, sizeof(buf), "%.0f%s", r.current, pct);
            } else {
                snprintf(buf, sizeof(buf), "%.0f%s / %.0f%s", r.current, pct, r.limit, pct);
            }
            // Unmet goals are pending; exceeded limits fail.
            const auto op = static_cast<codename::Op>(r.op);
            const bool reach = op == codename::Op::Ge || op == codename::Op::Gt;
            const bool near_limit = !reach && r.limit != 0 && r.current >= r.limit * kNearLimitShare;
            // Empty profiles have no spread reading.
            const ImVec4 color = r.pass ? (near_limit ? id_yellow : id_green)
                : !reach                ? (r.current > 0 ? id_red : unset_color())
                : r.limit > 0 && r.current >= r.limit * kCloseGoalShare ? id_yellow
                                                                             : unset_color();
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.label);
            ImGui::TableNextColumn();
            align_value(buf);
            ImGui::TextColored(color, "%s", buf);
        }
        const auto plain = [&](const char* key, int value) {
            format_count(value, buf, sizeof(buf));
            dim_row(key, buf);
        };
        if (stats.pw_camaraderie >= 0) {
            plain("camaraderie", stats.pw_camaraderie);
        }
        ImGui::EndTable();
    }

    if (ImGui::BeginTable("pw_class", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("takedowns", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("count", ImGuiTableColumnFlags_WidthFixed, ui_size(50));
        ImGui::TableSetupColumn("share", ImGuiTableColumnFlags_WidthFixed, ui_size(50));
        ImGui::TableHeadersRow();
        const auto share_row = [&](const char* label, int value, int total,
                                   const ImVec4& share_color) {
            const double share = total > 0 ? 100.0 * value / total : 0.0;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(value > 0 ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : pending,
                               "%s", label);
            ImGui::TableNextColumn();
            // Dim unused classes.
            format_count(value, buf, sizeof(buf));
            align_value(buf);
            if (value > 0) ImGui::TextUnformatted(buf);
            else ImGui::TextDisabled("0");
            ImGui::TableNextColumn();
            if (total > 0) snprintf(buf, sizeof(buf), "%.0f%%", share);
            else snprintf(buf, sizeof(buf), "-");
            align_value(buf);
            ImGui::TextColored(value > 0 ? share_color : pending, "%s", buf);
        };
        for (int cls = 0; cls < 6; ++cls) {
            share_row(codename::pw_class_name(cls), axes.by_class[cls], axes.total, id_yellow);
        }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("all");
        ImGui::TableNextColumn();
        format_count(axes.total, buf, sizeof(buf));
        align_value(buf);
        ImGui::TextDisabled("%s", buf);
        ImGui::TableNextColumn();
        align_value(axes.total > 0 ? "100%" : "-");
        ImGui::TextDisabled("%s", axes.total > 0 ? "100%" : "-");
        // Lethality is another view of same takedowns.
        ImGui::TableNextRow(ImGuiTableRowFlags_None, ui_size(6));
        // FOXHOUND is a non-lethal title: non-lethal must beat twice lethal.
        share_row("lethal", axes.lethal, axes.lethal + axes.nonlethal, id_red);
        share_row("non-lethal", axes.nonlethal, axes.lethal + axes.nonlethal,
                  codename::pw_nonlethal_beats_lethal(axes.lethal, axes.nonlethal) ? id_green
                                                                                   : id_red);
        ImGui::EndTable();
    }

    ImGui::Spacing();
    const codename::PwGrade grade = codename::pw_grade(stats);
    const bool coop = stats.pw_camaraderie > codename::kPwCoopCamaraderie;
    // Show first failing grade, held grade 5, or initial grade 1.
    const int target = grade.next ? grade.next : grade.grade ? grade.grade : 1;
    const codename::PwGradeGate gate = codename::pw_grade_gate(target);
    if (!stats.pw_codename_result_ok) {
        // Before evaluation, show grade 1 gates without status.
        ImGui::TextDisabled("No grade yet | finish a mission to be evaluated");
    } else if (grade.next) {
        ImGui::TextColored(grade.grade ? id_green : pending, "Grade %d / 5", grade.grade);
        ImGui::SameLine();
        ImGui::TextColored(id_yellow, "| next: %d", grade.next);
    } else {
        ImGui::TextColored(id_green, "Grade 5 / 5 | highest grade held");
    }
    char header[32];
    snprintf(header, sizeof(header), "grade %d needs", target);
    if (ImGui::BeginTable("pw_ladder", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn(header, ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("now", ImGuiTableColumnFlags_WidthFixed, ui_size(70));
        ImGui::TableSetupColumn("goal", ImGuiTableColumnFlags_WidthFixed, ui_size(70));
        ImGui::TableHeadersRow();
        // Unread or irrelevant gates have no failure status.
        const auto gate_row = [&](const char* label, bool known, bool judged,
                                  const char* now, const char* goal, bool pass) {
            const ImVec4 color = !known || !judged ? pending : pass ? id_green : id_red;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(judged ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : pending,
                               "%s", label);
            ImGui::TableNextColumn();
            align_value(known ? now : "-");
            ImGui::TextColored(color, "%s", known ? now : "-");
            ImGui::TableNextColumn();
            align_value(goal);
            ImGui::TextColored(color, "%s", goal);
        };
        char now_value[24], goal_value[32], gate_text[24];
        // Co-op requires above step; solo requires at or below.
        format_thousands(gate.camaraderie, gate_text, sizeof(gate_text));
        snprintf(goal_value, sizeof(goal_value), "%s%s", coop ? ">" : "<=", gate_text);
        format_thousands(stats.pw_camaraderie, now_value, sizeof(now_value));
        gate_row("camaraderie", stats.pw_camaraderie >= 0, true, now_value, goal_value,
                 coop ? stats.pw_camaraderie > gate.camaraderie
                      : stats.pw_camaraderie <= gate.camaraderie);
        // Only all-weapons titles (FOXHOUND among them) carry the Heroism floor.
        if (grade.all_weapons) {
            format_thousands(gate.heroism, gate_text, sizeof(gate_text));
            snprintf(goal_value, sizeof(goal_value), ">%s", gate_text);
        } else {
            snprintf(goal_value, sizeof(goal_value), "any");
        }
        format_thousands(stats.pw_heroism, now_value, sizeof(now_value));
        gate_row("heroism", true, grade.all_weapons, now_value, goal_value,
                 stats.pw_heroism > gate.heroism);
        // Grade 3 and up want the ratio to reach the gate, 1 and 2 to pass it.
        const bool ratio_known = stats.pw_codename_missions_required > 0;
        const double ratio = ratio_known
            ? static_cast<double>(stats.pw_codename_missions_counted)
                / stats.pw_codename_missions_required
            : 0.0;
        snprintf(now_value, sizeof(now_value), "%.2f", ratio);
        snprintf(goal_value, sizeof(goal_value), "%s%.2f", target >= 3 ? ">=" : ">",
                 gate.coop_ratio);
        gate_row("co-op ratio", ratio_known, true, now_value, goal_value,
                 target >= 3 ? ratio >= gate.coop_ratio : ratio > gate.coop_ratio);
        // Grades 4-5 also require native mission-rank verdict.
        if (target >= 4) {
            const bool flag = target == 5 ? stats.pw_codename_grade5_ok
                                          : stats.pw_codename_grade4_ok;
            gate_row("mission ranks", stats.pw_codename_result_ok, true,
                     flag ? "met" : "not met", "met", flag);
        }
        ImGui::EndTable();
    }
    if (stats.pw_codename_missions_required > 0) {
        ImGui::TextDisabled("ratio = %d / %d missions counted",
                            stats.pw_codename_missions_counted,
                            stats.pw_codename_missions_required);
    }
}

void draw_mgs3_captures(const GameStats& stats, int scroll)
{
    ImGui::Text("%d / 48", stats.plants_captured);
    checklist("captures", kMgs3Captures, std::size(kMgs3Captures), stats.capture_mask, scroll);
}

void draw_mgs3_kerotans(const GameStats& stats, int scroll)
{
    ImGui::Text("%d / 64", stats.kerotans);
    checklist("kerotans", kMgs3Kerotans, std::size(kMgs3Kerotans), stats.kerotan_mask, scroll);
}

void draw_summary(const GameStats& stats, int)
{
    const auto& match = g_eval.match;

    const auto [id_green, id_yellow, id_red] = id_colors(g_game);
    // Ghost Babel names its top rank per difficulty — HOUND, DOBERMAN, FOX,
    // BIG BOSS are all rank 0 — so the verdict comes from the kind, not the
    // name. Matching by name would paint Hard's FOX amber and Normal's
    // DOBERMAN as an also-ran.
    const ImVec4 codename_color = !match ? unset_color()
        : g_game == Game::Babel
            ? (match->kind == codename::Kind::Elite ? id_green
               : match->kind == codename::Kind::Worst ? id_red
                                                      : ImGui::GetStyleColorVec4(ImGuiCol_Text))
        : std::strcmp(match->name, "FOX") == 0 ? id_yellow
        : std::strcmp(match->name, "BIG BOSS") == 0 || std::strcmp(match->name, "FOXHOUND") == 0
            ? id_green : match->kind == codename::Kind::Worst ? id_red
            : ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImGui::SetWindowFontScale(2.0f);
    ImGui::TextColored(codename_color, "%s", match ? match->name : "---");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::BeginTable("reqs", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("requirement", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, ui_size(120));

        if (!stats.mgs1_japanese_original) {
            const bool classic_mg = g_game == Game::MG1 || g_game == Game::MG2;
            const bool known_difficulty = classic_mg
                || g_game == Game::Babel
                || g_game == Game::MGS1 || g_game == Game::MGS4
                || stats.difficulty_game_byte % 10 == 0;
            const bool valid_difficulty = known_difficulty
                && ((classic_mg && (stats.difficulty == Difficulty::Extreme
                                    || stats.difficulty == Difficulty::Easy))
                    || stats.difficulty == Difficulty::Extreme
                    || (g_game != Game::MGS1 && stats.difficulty == Difficulty::EuroExtreme));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("difficulty");
            ImGui::TableNextColumn();
            constexpr const char* babel_difficulties[]{"Easy", "Normal", "Hard", "Very Hard"};
            ImGui::TextColored(valid_difficulty ? id_green : id_red,
                               "%s%s", classic_mg
                                   ? (stats.difficulty == Difficulty::Extreme ? "Original" : "Easy")
                                   : g_game == Game::Babel && stats.difficulty_raw < 4
                                       ? babel_difficulties[stats.difficulty_raw]
                                   : difficulty_name(stats.difficulty),
                               known_difficulty ? "" : " (?)");
        }

        const auto& reqs = g_eval.reqs;
        for (const codename::ReqStatus& r : reqs) {
            char ratio[96];
            if (std::strcmp(r.label, "special items") == 0) {
                if (g_game == Game::MG1 || g_game == Game::MG2) {
                    snprintf(ratio, sizeof(ratio), "%s",
                             stats.special_item_used ? "Infinity Bandana" : "NONE");
                } else {
                    join_flags(ratio, sizeof(ratio), stats.special_items_mask,
                               special_item_names(g_game));
                }
            } else if (std::strcmp(r.label, "radar") == 0 && g_game == Game::MGS1) {
                snprintf(ratio, sizeof(ratio), "%s", stats.radar_off ? "OFF" : "ON");
            } else if (std::strcmp(r.label, "radar") == 0 && g_game == Game::MGS2) {
                const char* type = stats.radar_type == 0    ? "TYPE-A"
                                   : stats.radar_type == 0x20 ? "TYPE-B"
                                   : (stats.radar_type & 4) != 0 ? "OFF"
                                                              : "?";
                snprintf(ratio, sizeof(ratio), "%s (%s)",
                         stats.radar_off ? "UNUSED" : "USED", type);
            } else if (std::strcmp(r.label, "campaign") == 0 && g_game == Game::MGS2) {
                const char* selected = stats.mission == 0    ? "Plant"
                                       : stats.mission == 16 ? "Tanker"
                                       : stats.mission == 32 ? "Tanker + Plant"
                                                             : "?";
                snprintf(ratio, sizeof(ratio), "%s / Tanker + Plant", selected);
            } else switch (static_cast<codename::ReqFmt>(r.fmt)) {
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
            case codename::ReqFmt::Percent:
                snprintf(ratio, sizeof(ratio), "%.0f%% / %.0f%%", r.current, r.limit);
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
            const auto op = static_cast<codename::Op>(r.op);
            const bool near_limit = !over && (op == codename::Op::Le || op == codename::Op::Lt)
                && r.limit != 0 && r.current >= r.limit * kNearLimitShare;
            const ImVec4 state_col = over ? id_red : near_limit ? id_yellow : id_green;

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.label);
            ImGui::TableNextColumn();
            ImGui::TextColored(state_col, "%s", ratio);
        }

        auto plain_count = [&](const char* key, int value) {
            char buf[24];
            snprintf(buf, sizeof(buf), "%d", value);
            dim_row(key, buf);
        };

        auto plain_pair = [&](const char* key, int value, int maximum) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d / %d", value, maximum);
            dim_row(key, buf);
        };

        // One summary row for a pair that reads better inline.
        auto inline_pair = [&](const char* key, const char* left, int left_value,
                               const char* right, int right_value) {
            char buf[64];
            snprintf(buf, sizeof(buf), "%s %d | %s %d", left, left_value, right, right_value);
            dim_row(key, buf);
        };

        if (g_game == Game::MGS1) {
            plain_count("saves", stats.saves);
            plain_pair("health", stats.current_health, stats.max_health);
            if (stats.diazepam_frames > 0) {
                // The diazepam timer ticks at 30fps while game time runs at 60.
                constexpr double kMgs1TimerFps = 30.0;
                char buf[24];
                snprintf(buf, sizeof(buf), "%.1fs", stats.diazepam_frames / kMgs1TimerFps);
                dim_row("diazepam", buf);
            }
        } else if (g_game == Game::MGS2) {
            plain_count("dog tags", stats.dog_tags);
            plain_pair("health", stats.current_health, stats.max_health);
            dim_row("sea louse", stats.sea_louse ? "YES" : "NO");
            plain_count("clearing escapes", stats.clearing_escapes);
            plain_count("times seen", stats.times_seen);
            plain_count("mechs destroyed", stats.mechs_destroyed);
            inline_pair("pull-ups", "Snake", stats.snake_pull_ups, "Raiden",
                        stats.raiden_pull_ups);
        } else if (g_game == Game::MGS3) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d / 48", stats.plants_captured);
            dim_row("captures", buf);
            const int current_kerotan = mgs3_area_kerotan(stats.area_code);
            snprintf(buf, sizeof(buf), "%d / 64  %s", stats.kerotans,
                     current_kerotan >= 0
                             && (stats.kerotan_mask & (uint64_t{1} << current_kerotan)) != 0
                         ? "x" : "-");
            dim_row("kerotans", buf);
            plain_count("meals eaten", stats.meals_eaten);
        } else if (g_game == Game::MGS4) {
            plain_count("flashbacks", stats.flashbacks_viewed);
        }
        ImGui::EndTable();
    }

    if (stats.area_code[0]) {
        char buf[128];
        const char* area = area_name(g_game, stats.area_code);
        const char* state = g_game == Game::MGS2 ? mgs2_area_state(stats) : nullptr;
        if (area && state) {
            snprintf(buf, sizeof(buf), "%s (%s) (%s)", area, state, stats.area_code);
        } else {
            snprintf(buf, sizeof(buf), area ? "%s (%s)" : "%s", area ? area : stats.area_code,
                     stats.area_code);
        }
        if (g_game == Game::MGS2) {
            char stage_time[16];
            format_time(stats.stage_time_seconds, stage_time, sizeof(stage_time));
            const size_t used = std::strlen(buf);
            snprintf(buf + used, sizeof(buf) - used, " | %s", stage_time);
        }
        ImGui::Spacing();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", buf);
        ImGui::PopTextWrapPos();
    }

}

using PanelTabDraw = void (*)(const GameStats&, int);

struct PanelTab {
    const char* title;
    PanelTabDraw draw;
};

constexpr PanelTab kGenericTabs[] = {{"Summary", draw_summary}};
constexpr PanelTab kMgs2Tabs[] = {{"Summary", draw_summary}, {"Dog Tags", draw_mgs2_dog_tags}};
constexpr PanelTab kMgs3Tabs[] = {{"Summary", draw_summary},
                                  {"Capture", draw_mgs3_captures},
                                  {"Kerotan", draw_mgs3_kerotans}};
constexpr PanelTab kMgs4Tabs[] = {{"Summary", draw_summary}, {"Feats", draw_mgs4_feats}};
constexpr PanelTab kMgspwTabs[] = {{"Summary", draw_mgspw_summary},
                                   {"Career", draw_mgspw_global},
                                   {"Insignia", draw_mgspw_insignia},
                                   {"Codenames", draw_mgspw_codenames}};

std::span<const PanelTab> panel_tabs(Game game)
{
    switch (game) {
    case Game::MGS2: return kMgs2Tabs;
    case Game::MGS3: return kMgs3Tabs;
    case Game::MGS4: return kMgs4Tabs;
    case Game::MGSPW: return kMgspwTabs;
    default: return kGenericTabs;
    }
}

void draw_panel()
{
    static GameStats stats{};
    static bool have_stats = false;
    static uint64_t next_poll = 0;
    const uint64_t now = GetTickCount64();
    if (now >= next_poll) {
        AcquireSRWLockShared(&g_stats_lock);
        stats = g_stats;
        have_stats = g_have_stats;
        ReleaseSRWLockShared(&g_stats_lock);
        if (have_stats) {
            const auto eval_fn = kEvaluateFns[static_cast<int>(g_game)];
            g_eval.match = eval_fn ? eval_fn(stats) : std::optional<codename::Match>{};
            const auto req_fn = kRequirementsFns[static_cast<int>(g_game)];
            g_eval.reqs = req_fn ? req_fn(stats) : std::vector<codename::ReqStatus>{};
            g_eval.matches = g_game == Game::MGS4 ? codename::all_matches_mgs4(stats)
                                                  : std::vector<codename::Match>{};
        }
        next_poll = now + 100;
    }
    const char* panel_title = g_game == Game::MGS3 || g_game == Game::MGSPW
        ? "FOXHOUND tracker"
        : "BIG BOSS tracker";
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiCond initial_layout = g.reset_window ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
    const ImVec2 anchor(viewport->WorkPos.x + ui_size(16),
                        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(anchor, initial_layout, ImVec2(0.0f, 0.5f));
    ImGui::Begin(panel_title, nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
                     | ImGuiWindowFlags_AlwaysAutoResize);
    static float placed_height = -1.0f;
    const float panel_height = ImGui::GetWindowHeight();
    if (panel_height != placed_height) {
        ImGui::SetWindowPos(ImVec2(anchor.x, anchor.y - panel_height * 0.5f));
        placed_height = panel_height;
    }
    g.reset_window = false;

    if (!have_stats) {
        ImGui::TextDisabled("no active ranked run");
        ImGui::End();
        return;
    }

    static int selected_tab = 0;
    const std::span<const PanelTab> tabs = panel_tabs(g_game);
    const bool tab_bar = tabs.size() > 1;
    if (tab_bar && key_pressed(kTabKey)) {
        selected_tab = (selected_tab + 1) % static_cast<int>(tabs.size());
    }
    const bool scroll_up = key_pressed(VK_UP);
    const bool scroll_down = key_pressed(VK_DOWN);
    const int scroll = scroll_up ? -1 : scroll_down ? 1 : 0;
    if (tab_bar && ImGui::BeginTabBar("tracker_tabs")) {
        for (int i = 0; i < static_cast<int>(tabs.size()); ++i) {
            if (ImGui::BeginTabItem(tabs[i].title, nullptr,
                                    selected_tab == i ? ImGuiTabItemFlags_SetSelected : 0)) {
                tabs[i].draw(stats, scroll);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    } else {
        tabs[0].draw(stats, scroll);
    }

    ImGui::End();
}

void STDMETHODCALLTYPE hk_execute_command_lists(ID3D12CommandQueue* queue, UINT count,
                                                 ID3D12CommandList* const* lists)
{
    if (!g.queue12.load() && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
        queue->AddRef();
        ID3D12CommandQueue* expected = nullptr;
        if (!g.queue12.compare_exchange_strong(expected, queue)) {
            queue->Release();
        } else {
            LOG_INFO("captured D3D12 direct command queue %p", reinterpret_cast<void*>(queue));
        }
    }
    oExecuteCommandLists(queue, count, lists);
}

bool render_d3d12(IDXGISwapChain* swap_chain)
{
    IDXGISwapChain3* swap_chain3 = nullptr;
    if (FAILED(swap_chain->QueryInterface(IID_PPV_ARGS(&swap_chain3)))) return false;
    const UINT index = swap_chain3->GetCurrentBackBufferIndex();
    swap_chain3->Release();
    if (index >= g.frames12.size()) return false;

    D3D12Frame& frame = g.frames12[index];
    // A GPU more than a frame behind means saturation, not overlay work:
    // skip this frame's overlay rather than stall Present behind it.
    constexpr DWORD kFenceWaitMs = 100;
    if (!wait_d3d12(&frame, kFenceWaitMs)) return false;
    if (FAILED(frame.allocator->Reset())
        || FAILED(g.command_list12->Reset(frame.allocator, nullptr))) return false;

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = frame.buffer;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g.command_list12->ResourceBarrier(1, &barrier);
    g.command_list12->OMSetRenderTargets(1, &frame.rtv, FALSE, nullptr);
    g.command_list12->SetDescriptorHeaps(1, &g.srv_heap12);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g.command_list12);
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    g.command_list12->ResourceBarrier(1, &barrier);
    if (FAILED(g.command_list12->Close())) return false;

    ID3D12CommandList* lists[] = {g.command_list12};
    ID3D12CommandQueue* queue = g.queue12.load();
    queue->ExecuteCommandLists(1, lists);
    const UINT64 fence = ++g.next_fence12;
    if (FAILED(queue->Signal(g.fence12, fence))) return false;
    frame.fence_value = fence;
    return true;
}

HRESULT STDMETHODCALLTYPE hk_present(IDXGISwapChain* swap_chain, UINT sync_interval, UINT flags)
{
    poll_toggle_key();
    if (!g.show) {
        return oPresent(swap_chain, sync_interval, flags);
    }
    if (!g.imgui_ready) {
        ID3D11Device* device11 = nullptr;
        const bool is_d3d11 = SUCCEEDED(swap_chain->GetDevice(IID_PPV_ARGS(&device11)));
        if (device11) device11->Release();
        if (!(is_d3d11 ? init_imgui(swap_chain) : init_imgui_d3d12(swap_chain))) {
            return oPresent(swap_chain, sync_interval, flags);
        }
    }

    if (g.renderer == Renderer::D3D11) ImGui_ImplDX11_NewFrame();
    else ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    // Win32 reports client size, which can differ from swap-chain size under
    // internal scaling. ImGui draws into the back buffer, so use its size.
    ImGui::GetIO().DisplaySize = g.render_size;
    apply_ui_scale();
    ImGui::NewFrame();
    draw_panel();
    ImGui::Render();

    if (g.renderer == Renderer::D3D11) {
        g.context->OMSetRenderTargets(1, &g.rtv, nullptr);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    } else {
        render_d3d12(swap_chain);
    }

    return oPresent(swap_chain, sync_interval, flags);
}

HRESULT STDMETHODCALLTYPE hk_resize_buffers(IDXGISwapChain* swap_chain, UINT buffer_count, UINT width,
                                            UINT height, DXGI_FORMAT format, UINT flags)
{
    if (g.imgui_ready && g.renderer == Renderer::D3D12) {
        release_d3d12();
    } else if (g.imgui_ready) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
        release_rtv();
    }
    HRESULT hr = oResizeBuffers(swap_chain, buffer_count, width, height, format, flags);
    if (SUCCEEDED(hr) && g.imgui_ready && !g.rtv) {
        create_rtv(swap_chain);
    }
    return hr;
}

bool hook_targets(void* present_target, void* resize_target, void* execute_target = nullptr)
{
    // Shared with qcamo: serialized installs let MinHook chain both detours.
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\MGSModsDxgiHookInstall");
    DWORD wait = mutex ? WaitForSingleObject(mutex, INFINITE) : WAIT_FAILED;
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) {
        if (mutex) CloseHandle(mutex);
        LOG_ERROR("DXGI hook mutex failed");
        return false;
    }

    bool ok = true;
    if (MH_Initialize() != MH_OK) {
        LOG_ERROR("MH_Initialize failed");
        ok = false;
    } else if (MH_CreateHook(present_target, reinterpret_cast<void*>(&hk_present),
                             reinterpret_cast<void**>(&oPresent)) != MH_OK
               || MH_CreateHook(resize_target, reinterpret_cast<void*>(&hk_resize_buffers),
                                reinterpret_cast<void**>(&oResizeBuffers)) != MH_OK
               || (execute_target
                   && MH_CreateHook(execute_target,
                                    reinterpret_cast<void*>(&hk_execute_command_lists),
                                    reinterpret_cast<void**>(&oExecuteCommandLists)) != MH_OK)) {
        LOG_ERROR("MH_CreateHook failed");
        ok = false;
    } else if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        LOG_ERROR("MH_EnableHook failed");
        ok = false;
    }
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return ok;
}

bool install_d3d11_hooks()
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

    return hook_targets(present_target, resize_target);
}

bool install_d3d12_hooks()
{
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = "BBTrackerDummyWnd12";
    if (!RegisterClassExA(&wc)) return false;
    HWND wnd = CreateWindowExA(0, wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100,
                               nullptr, nullptr, wc.hInstance, nullptr);

    IDXGIFactory4* factory = nullptr;
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    IDXGISwapChain1* swap_chain1 = nullptr;
    IDXGISwapChain3* swap_chain3 = nullptr;
    bool ok = wnd && SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))
        && SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (ok) ok = SUCCEEDED(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue)));
    DXGI_SWAP_CHAIN_DESC1 swap_desc{};
    swap_desc.Width = 100;
    swap_desc.Height = 100;
    swap_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_desc.SampleDesc.Count = 1;
    swap_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.BufferCount = 2;
    swap_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    if (ok) ok = SUCCEEDED(factory->CreateSwapChainForHwnd(queue, wnd, &swap_desc, nullptr, nullptr,
                                                            &swap_chain1))
        && SUCCEEDED(swap_chain1->QueryInterface(IID_PPV_ARGS(&swap_chain3)));

    void* present_target = nullptr;
    void* resize_target = nullptr;
    void* execute_target = nullptr;
    if (ok) {
        void** swap_vtable = *reinterpret_cast<void***>(swap_chain3);
        void** queue_vtable = *reinterpret_cast<void***>(queue);
        present_target = swap_vtable[kPresentIndex];
        resize_target = swap_vtable[kResizeBuffersIndex];
        execute_target = queue_vtable[kExecuteCommandListsIndex];
    } else {
        LOG_ERROR("dummy d3d12 device creation failed");
    }

    if (swap_chain3) swap_chain3->Release();
    if (swap_chain1) swap_chain1->Release();
    if (queue) queue->Release();
    if (device) device->Release();
    if (factory) factory->Release();
    if (wnd) DestroyWindow(wnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);
    return ok && hook_targets(present_target, resize_target, execute_target);
}

std::filesystem::path dll_dir()
{
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(own_module(), path, MAX_PATH);
    return std::filesystem::path(path).parent_path();
}

} // namespace

void start_overlay(const char* game_label, StatsFn stats_fn, const wchar_t* game_module,
                   Game game, ClockFn clock_fn)
{
    g_label = game_label;
    g_stats_fn = stats_fn;
    g_clock_fn = clock_fn;
    g_game = game;

    std::filesystem::path dir = dll_dir();
    std::string log_path = (dir / L"bbtracker.log").string();

    if (!log_init(log_path.c_str())) {
        return;
    }
    LOG_INFO("bbtracker starting (%s), toggle key vk=0x%02X", g_label, kToggleKey);

    bool logged_wait_module = false;
    bool logged_wait_renderer = false;
    bool use_d3d12 = false;
    for (;;) {
        if (!GetModuleHandleW(game_module)) {
            if (!logged_wait_module) {
                LOG_INFO("waiting for game module %S", game_module);
                logged_wait_module = true;
            }
            Sleep(500);
            continue;
        }
        HMODULE d3d11 = GetModuleHandleW(L"d3d11.dll");
        HMODULE d3d12 = GetModuleHandleW(L"d3d12.dll");
        if (!d3d11 && !d3d12) {
            if (!logged_wait_renderer) {
                LOG_INFO("waiting for d3d11.dll or d3d12.dll");
                logged_wait_renderer = true;
            }
            Sleep(500);
            continue;
        }
        use_d3d12 = !d3d11 && d3d12;
        break;
    }

    while (!(use_d3d12 ? install_d3d12_hooks() : install_d3d11_hooks())) {
        LOG_WARN("hook install failed, retrying in 5s");
        Sleep(5000);
    }

    LOG_INFO("D3D%d hooks installed", use_d3d12 ? 12 : 11);

    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    uint64_t total_ticks = 0;
    uint64_t max_ticks = 0;
    unsigned timing_samples = 0;
    for (;;) {
        LARGE_INTEGER started{};
        LARGE_INTEGER finished{};
        QueryPerformanceCounter(&started);
        GameStats stats{};
        const bool have_stats = g_stats_fn && g_stats_fn(stats);
        QueryPerformanceCounter(&finished);
        const uint64_t elapsed = static_cast<uint64_t>(finished.QuadPart - started.QuadPart);
        total_ticks += elapsed;
        max_ticks = elapsed > max_ticks ? elapsed : max_ticks;
        AcquireSRWLockExclusive(&g_stats_lock);
        g_stats = stats;
        g_have_stats = have_stats;
        ReleaseSRWLockExclusive(&g_stats_lock);
        if (++timing_samples == 240) {
            LOG_INFO("probe timing: avg=%lluus max=%lluus",
                     static_cast<unsigned long long>(total_ticks * 1000000
                                                     / frequency.QuadPart / timing_samples),
                     static_cast<unsigned long long>(max_ticks * 1000000
                                                     / frequency.QuadPart));
            total_ticks = 0;
            max_ticks = 0;
            timing_samples = 0;
        }
        // 10 Hz: the live per-mission tallies are read here, and at 4 Hz the
        // sampling lag was visible in game. PW probe costs ~80us per poll.
        Sleep(100);
    }
}

} // namespace bb
