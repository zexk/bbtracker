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
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <array>
#include <optional>
#include <vector>
#include <filesystem>

#include "../common/codename/codename.h"
#include "../common/log.h"

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
    ID3D12CommandQueue* queue12 = nullptr;
    ID3D12GraphicsCommandList* command_list12 = nullptr;
    ID3D12DescriptorHeap* rtv_heap12 = nullptr;
    ID3D12DescriptorHeap* srv_heap12 = nullptr;
    ID3D12Fence* fence12 = nullptr;
    HANDLE fence_event12 = nullptr;
    UINT64 next_fence12 = 0;
    std::vector<D3D12Frame> frames12;
    HWND hwnd = nullptr;
};

const char* g_label = "?";
StatsFn g_stats_fn = nullptr;
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

void wait_d3d12(D3D12Frame* frame = nullptr)
{
    const UINT64 value = frame ? frame->fence_value : g.next_fence12;
    if (!value || !g.fence12 || g.fence12->GetCompletedValue() >= value) {
        return;
    }
    if (SUCCEEDED(g.fence12->SetEventOnCompletion(value, g.fence_event12))) {
        WaitForSingleObject(g.fence_event12, INFINITE);
    }
}

void release_d3d12()
{
    wait_d3d12();
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
    if (!g.queue12
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

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
    apply_game_theme();
    ImGui_ImplWin32_Init(g.hwnd);
    ImGui_ImplDX12_InitInfo info{};
    info.Device = g.device12;
    info.CommandQueue = g.queue12;
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

void apply_game_theme()
{
    ImGui::StyleColorsDark();
    if (g_game != Game::MG1 && g_game != Game::MG2
        && g_game != Game::MGS1 && g_game != Game::MGS2
        && g_game != Game::MGS3 && g_game != Game::MGS4
        && g_game != Game::MGSPW) {
        return;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;

    ImVec4* colors = style.Colors;
    if (g_game == Game::MG1) {
        colors[ImGuiCol_Text]              = ImVec4(0.72f, 0.73f, 0.72f, 1.00f);
        colors[ImGuiCol_TextDisabled]      = ImVec4(0.43f, 0.44f, 0.43f, 1.00f);
        colors[ImGuiCol_WindowBg]          = ImVec4(0.01f, 0.01f, 0.01f, 0.80f);
        colors[ImGuiCol_Border]            = ImVec4(0.58f, 0.59f, 0.57f, 0.72f);
        colors[ImGuiCol_FrameBg]           = ImVec4(0.16f, 0.16f, 0.15f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]    = ImVec4(0.40f, 0.18f, 0.04f, 1.00f);
        colors[ImGuiCol_FrameBgActive]     = ImVec4(0.62f, 0.12f, 0.06f, 1.00f);
        colors[ImGuiCol_TitleBg]           = ImVec4(0.22f, 0.03f, 0.02f, 1.00f);
        colors[ImGuiCol_TitleBgActive]     = ImVec4(0.55f, 0.07f, 0.04f, 1.00f);
        colors[ImGuiCol_CheckMark]         = ImVec4(0.80f, 0.48f, 0.04f, 1.00f);
        colors[ImGuiCol_SliderGrab]        = ImVec4(0.58f, 0.59f, 0.57f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]  = ImVec4(0.80f, 0.48f, 0.04f, 1.00f);
        colors[ImGuiCol_Button]            = ImVec4(0.20f, 0.20f, 0.19f, 1.00f);
        colors[ImGuiCol_ButtonHovered]     = ImVec4(0.40f, 0.18f, 0.04f, 1.00f);
        colors[ImGuiCol_ButtonActive]      = ImVec4(0.62f, 0.12f, 0.06f, 1.00f);
        colors[ImGuiCol_Header]            = ImVec4(0.20f, 0.20f, 0.19f, 1.00f);
        colors[ImGuiCol_HeaderHovered]     = ImVec4(0.40f, 0.18f, 0.04f, 1.00f);
        colors[ImGuiCol_HeaderActive]      = ImVec4(0.62f, 0.12f, 0.06f, 1.00f);
        colors[ImGuiCol_Separator]         = ImVec4(0.58f, 0.59f, 0.57f, 0.68f);
        colors[ImGuiCol_TableRowBgAlt]     = ImVec4(0.16f, 0.16f, 0.15f, 0.70f);
        colors[ImGuiCol_ResizeGrip]        = ImVec4(0.62f, 0.12f, 0.06f, 0.35f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.80f, 0.48f, 0.04f, 0.75f);
        colors[ImGuiCol_ResizeGripActive]  = ImVec4(0.80f, 0.48f, 0.04f, 1.00f);
        return;
    }

    if (g_game == Game::MGS1) {
        colors[ImGuiCol_Text]              = ImVec4(0.55f, 0.76f, 0.69f, 1.00f);
        colors[ImGuiCol_TextDisabled]      = ImVec4(0.32f, 0.47f, 0.43f, 1.00f);
        colors[ImGuiCol_WindowBg]          = ImVec4(0.03f, 0.07f, 0.07f, 0.80f);
        colors[ImGuiCol_Border]            = ImVec4(0.43f, 0.72f, 0.64f, 0.65f);
        colors[ImGuiCol_FrameBg]           = ImVec4(0.08f, 0.17f, 0.16f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]    = ImVec4(0.20f, 0.43f, 0.38f, 1.00f);
        colors[ImGuiCol_FrameBgActive]     = ImVec4(0.30f, 0.58f, 0.51f, 1.00f);
        colors[ImGuiCol_TitleBg]           = ImVec4(0.05f, 0.13f, 0.13f, 1.00f);
        colors[ImGuiCol_TitleBgActive]     = ImVec4(0.12f, 0.27f, 0.25f, 1.00f);
        colors[ImGuiCol_CheckMark]         = ImVec4(0.60f, 0.94f, 0.82f, 1.00f);
        colors[ImGuiCol_SliderGrab]        = ImVec4(0.43f, 0.72f, 0.64f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]  = ImVec4(0.60f, 0.94f, 0.82f, 1.00f);
        colors[ImGuiCol_Button]            = ImVec4(0.10f, 0.23f, 0.21f, 1.00f);
        colors[ImGuiCol_ButtonHovered]     = ImVec4(0.20f, 0.43f, 0.38f, 1.00f);
        colors[ImGuiCol_ButtonActive]      = ImVec4(0.30f, 0.58f, 0.51f, 1.00f);
        colors[ImGuiCol_Header]            = ImVec4(0.10f, 0.23f, 0.21f, 1.00f);
        colors[ImGuiCol_HeaderHovered]     = ImVec4(0.20f, 0.43f, 0.38f, 1.00f);
        colors[ImGuiCol_HeaderActive]      = ImVec4(0.30f, 0.58f, 0.51f, 1.00f);
        colors[ImGuiCol_Separator]         = ImVec4(0.43f, 0.72f, 0.64f, 0.65f);
        colors[ImGuiCol_TableRowBgAlt]     = ImVec4(0.08f, 0.18f, 0.17f, 0.55f);
        colors[ImGuiCol_ResizeGrip]        = ImVec4(0.43f, 0.72f, 0.64f, 0.30f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.60f, 0.94f, 0.82f, 0.70f);
        colors[ImGuiCol_ResizeGripActive]  = ImVec4(0.60f, 0.94f, 0.82f, 1.00f);
        return;
    }

    if (g_game == Game::MG2) {
        colors[ImGuiCol_Text]              = ImVec4(0.72f, 0.75f, 0.76f, 1.00f);
        colors[ImGuiCol_TextDisabled]      = ImVec4(0.42f, 0.46f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg]          = ImVec4(0.00f, 0.01f, 0.03f, 0.80f);
        colors[ImGuiCol_Border]            = ImVec4(0.12f, 0.30f, 0.78f, 0.80f);
        colors[ImGuiCol_FrameBg]           = ImVec4(0.03f, 0.07f, 0.17f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]    = ImVec4(0.08f, 0.20f, 0.48f, 1.00f);
        colors[ImGuiCol_FrameBgActive]     = ImVec4(0.12f, 0.30f, 0.70f, 1.00f);
        colors[ImGuiCol_TitleBg]           = ImVec4(0.02f, 0.05f, 0.16f, 1.00f);
        colors[ImGuiCol_TitleBgActive]     = ImVec4(0.04f, 0.11f, 0.34f, 1.00f);
        colors[ImGuiCol_CheckMark]         = ImVec4(0.42f, 0.62f, 0.94f, 1.00f);
        colors[ImGuiCol_SliderGrab]        = ImVec4(0.25f, 0.43f, 0.78f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]  = ImVec4(0.42f, 0.62f, 0.94f, 1.00f);
        colors[ImGuiCol_Button]            = ImVec4(0.04f, 0.10f, 0.25f, 1.00f);
        colors[ImGuiCol_ButtonHovered]     = ImVec4(0.08f, 0.20f, 0.48f, 1.00f);
        colors[ImGuiCol_ButtonActive]      = ImVec4(0.12f, 0.30f, 0.70f, 1.00f);
        colors[ImGuiCol_Header]            = ImVec4(0.04f, 0.10f, 0.25f, 1.00f);
        colors[ImGuiCol_HeaderHovered]     = ImVec4(0.08f, 0.20f, 0.48f, 1.00f);
        colors[ImGuiCol_HeaderActive]      = ImVec4(0.12f, 0.30f, 0.70f, 1.00f);
        colors[ImGuiCol_Separator]         = ImVec4(0.12f, 0.30f, 0.78f, 0.75f);
        colors[ImGuiCol_TableRowBgAlt]     = ImVec4(0.03f, 0.07f, 0.17f, 0.72f);
        colors[ImGuiCol_ResizeGrip]        = ImVec4(0.12f, 0.30f, 0.78f, 0.35f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.42f, 0.62f, 0.94f, 0.75f);
        colors[ImGuiCol_ResizeGripActive]  = ImVec4(0.42f, 0.62f, 0.94f, 1.00f);
        return;
    }

    if (g_game == Game::MGS2) {
        colors[ImGuiCol_Text]              = ImVec4(0.61f, 0.69f, 0.64f, 1.00f);
        colors[ImGuiCol_TextDisabled]      = ImVec4(0.34f, 0.42f, 0.38f, 1.00f);
        colors[ImGuiCol_WindowBg]          = ImVec4(0.02f, 0.05f, 0.04f, 0.80f);
        colors[ImGuiCol_Border]            = ImVec4(0.38f, 0.53f, 0.47f, 0.70f);
        colors[ImGuiCol_FrameBg]           = ImVec4(0.07f, 0.13f, 0.11f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]    = ImVec4(0.28f, 0.10f, 0.07f, 1.00f);
        colors[ImGuiCol_FrameBgActive]     = ImVec4(0.48f, 0.13f, 0.08f, 1.00f);
        colors[ImGuiCol_TitleBg]           = ImVec4(0.04f, 0.09f, 0.07f, 1.00f);
        colors[ImGuiCol_TitleBgActive]     = ImVec4(0.20f, 0.08f, 0.06f, 1.00f);
        colors[ImGuiCol_CheckMark]         = ImVec4(0.76f, 0.19f, 0.11f, 1.00f);
        colors[ImGuiCol_SliderGrab]        = ImVec4(0.48f, 0.61f, 0.55f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]  = ImVec4(0.76f, 0.19f, 0.11f, 1.00f);
        colors[ImGuiCol_Button]            = ImVec4(0.10f, 0.18f, 0.15f, 1.00f);
        colors[ImGuiCol_ButtonHovered]     = ImVec4(0.28f, 0.10f, 0.07f, 1.00f);
        colors[ImGuiCol_ButtonActive]      = ImVec4(0.48f, 0.13f, 0.08f, 1.00f);
        colors[ImGuiCol_Header]            = ImVec4(0.10f, 0.18f, 0.15f, 1.00f);
        colors[ImGuiCol_HeaderHovered]     = ImVec4(0.28f, 0.10f, 0.07f, 1.00f);
        colors[ImGuiCol_HeaderActive]      = ImVec4(0.48f, 0.13f, 0.08f, 1.00f);
        colors[ImGuiCol_Separator]         = ImVec4(0.38f, 0.53f, 0.47f, 0.70f);
        colors[ImGuiCol_TableRowBgAlt]     = ImVec4(0.08f, 0.15f, 0.12f, 0.55f);
        colors[ImGuiCol_ResizeGrip]        = ImVec4(0.48f, 0.61f, 0.55f, 0.30f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.76f, 0.19f, 0.11f, 0.70f);
        colors[ImGuiCol_ResizeGripActive]  = ImVec4(0.76f, 0.19f, 0.11f, 1.00f);
        return;
    }

    if (g_game == Game::MGS4) {
        style.ScrollbarRounding = 0.0f;
        colors[ImGuiCol_Text]              = ImVec4(0.84f, 0.83f, 0.66f, 1.00f);
        colors[ImGuiCol_TextDisabled]      = ImVec4(0.49f, 0.48f, 0.36f, 1.00f);
        colors[ImGuiCol_WindowBg]          = ImVec4(0.08f, 0.08f, 0.05f, 0.96f);
        colors[ImGuiCol_Border]            = ImVec4(0.52f, 0.48f, 0.31f, 0.72f);
        colors[ImGuiCol_FrameBg]           = ImVec4(0.17f, 0.16f, 0.10f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]    = ImVec4(0.42f, 0.30f, 0.08f, 1.00f);
        colors[ImGuiCol_FrameBgActive]     = ImVec4(0.62f, 0.42f, 0.08f, 1.00f);
        colors[ImGuiCol_TitleBg]           = ImVec4(0.13f, 0.12f, 0.07f, 1.00f);
        colors[ImGuiCol_TitleBgActive]     = ImVec4(0.32f, 0.27f, 0.12f, 1.00f);
        colors[ImGuiCol_CheckMark]         = ImVec4(0.94f, 0.66f, 0.14f, 1.00f);
        colors[ImGuiCol_SliderGrab]        = ImVec4(0.58f, 0.53f, 0.32f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]  = ImVec4(0.94f, 0.66f, 0.14f, 1.00f);
        colors[ImGuiCol_Button]            = ImVec4(0.21f, 0.20f, 0.12f, 1.00f);
        colors[ImGuiCol_ButtonHovered]     = ImVec4(0.42f, 0.30f, 0.08f, 1.00f);
        colors[ImGuiCol_ButtonActive]      = ImVec4(0.62f, 0.42f, 0.08f, 1.00f);
        colors[ImGuiCol_Header]            = ImVec4(0.21f, 0.20f, 0.12f, 1.00f);
        colors[ImGuiCol_HeaderHovered]     = ImVec4(0.42f, 0.30f, 0.08f, 1.00f);
        colors[ImGuiCol_HeaderActive]      = ImVec4(0.62f, 0.42f, 0.08f, 1.00f);
        colors[ImGuiCol_Tab]               = ImVec4(0.17f, 0.16f, 0.10f, 1.00f);
        colors[ImGuiCol_TabHovered]        = ImVec4(0.42f, 0.30f, 0.08f, 1.00f);
        colors[ImGuiCol_TabActive]         = ImVec4(0.62f, 0.42f, 0.08f, 1.00f);
        colors[ImGuiCol_TabUnfocused]      = ImVec4(0.13f, 0.12f, 0.07f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.32f, 0.27f, 0.12f, 1.00f);
        colors[ImGuiCol_Separator]         = ImVec4(0.52f, 0.48f, 0.31f, 0.68f);
        colors[ImGuiCol_TableRowBgAlt]     = ImVec4(0.20f, 0.19f, 0.12f, 0.60f);
        colors[ImGuiCol_ResizeGrip]        = ImVec4(0.62f, 0.42f, 0.08f, 0.35f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.94f, 0.66f, 0.14f, 0.75f);
        colors[ImGuiCol_ResizeGripActive]  = ImVec4(0.94f, 0.66f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]       = ImVec4(0.10f, 0.10f, 0.06f, 0.85f);
        colors[ImGuiCol_ScrollbarGrab]     = ImVec4(0.38f, 0.35f, 0.21f, 0.90f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.62f, 0.42f, 0.08f, 0.95f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.94f, 0.66f, 0.14f, 1.00f);
        return;
    }

    colors[ImGuiCol_Text]                 = ImVec4(0.86f, 0.87f, 0.76f, 1.00f);
    colors[ImGuiCol_TextDisabled]         = ImVec4(0.48f, 0.49f, 0.42f, 1.00f);
    style.ScrollbarRounding = 0.0f;
    colors[ImGuiCol_WindowBg]             = ImVec4(0.09f, 0.10f, 0.08f, 0.80f);
    colors[ImGuiCol_Border]               = ImVec4(0.38f, 0.39f, 0.31f, 0.65f);
    colors[ImGuiCol_FrameBg]              = ImVec4(0.20f, 0.21f, 0.17f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.29f, 0.30f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.36f, 0.37f, 0.29f, 1.00f);
    colors[ImGuiCol_TitleBg]              = ImVec4(0.15f, 0.16f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.31f, 0.33f, 0.25f, 1.00f);
    colors[ImGuiCol_CheckMark]            = ImVec4(0.72f, 0.75f, 0.51f, 1.00f);
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.55f, 0.57f, 0.41f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.72f, 0.75f, 0.51f, 1.00f);
    colors[ImGuiCol_Button]               = ImVec4(0.25f, 0.26f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.36f, 0.37f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.44f, 0.45f, 0.33f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(0.25f, 0.26f, 0.20f, 1.00f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.36f, 0.37f, 0.28f, 1.00f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.44f, 0.45f, 0.33f, 1.00f);
    colors[ImGuiCol_Tab]                  = ImVec4(0.20f, 0.21f, 0.17f, 1.00f);
    colors[ImGuiCol_TabHovered]           = ImVec4(0.36f, 0.37f, 0.28f, 1.00f);
    colors[ImGuiCol_TabActive]            = ImVec4(0.44f, 0.45f, 0.33f, 1.00f);
    colors[ImGuiCol_TabUnfocused]         = ImVec4(0.15f, 0.16f, 0.12f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive]   = ImVec4(0.31f, 0.33f, 0.25f, 1.00f);
    colors[ImGuiCol_Separator]            = ImVec4(0.38f, 0.39f, 0.31f, 0.65f);
    colors[ImGuiCol_TableRowBgAlt]        = ImVec4(0.22f, 0.23f, 0.19f, 0.55f);
    colors[ImGuiCol_ResizeGrip]           = ImVec4(0.55f, 0.57f, 0.41f, 0.30f);
    colors[ImGuiCol_ResizeGripHovered]    = ImVec4(0.72f, 0.75f, 0.51f, 0.70f);
    colors[ImGuiCol_ResizeGripActive]     = ImVec4(0.72f, 0.75f, 0.51f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.12f, 0.13f, 0.10f, 0.70f);
    colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.38f, 0.39f, 0.28f, 0.85f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.55f, 0.57f, 0.41f, 0.90f);
    colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.72f, 0.75f, 0.51f, 0.95f);
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
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
    apply_game_theme();
    ImGui_ImplWin32_Init(g.hwnd);
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

const char* alert_state_name(uint8_t state)
{
    switch (state) {
    case 0: return "clear";
    case 1: return "alert";
    case 2: return "evasion";
    case 3: return "caution";
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

const char* exact_area_name(const char* code, const AreaName* areas, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        if (std::strcmp(code, areas[i].code) == 0) {
            return areas[i].name;
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
    return exact_area_name(code, kAreas, std::size(kAreas));
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
    return exact_area_name(code, kAreas, std::size(kAreas));
}

const char* mgs4_area_name(const char* code)
{
    constexpr AreaName kAreas[] = {
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
    if (const char* area = exact_area_name(code, kAreas, std::size(kAreas))) return area;
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

IdColors id_colors(Game game)
{
    switch (game) {
    case Game::MG1:  return {{0.48f, 0.72f, 0.38f, 1}, {0.80f, 0.48f, 0.04f, 1}, {0.82f, 0.20f, 0.10f, 1}};
    case Game::MG2:  return {{0.38f, 0.72f, 0.52f, 1}, {0.82f, 0.66f, 0.20f, 1}, {0.85f, 0.28f, 0.28f, 1}};
    case Game::MGS1: return {{0.42f, 0.88f, 0.66f, 1}, {0.88f, 0.72f, 0.28f, 1}, {0.90f, 0.32f, 0.30f, 1}};
    case Game::MGS2: return {{0.42f, 0.82f, 0.52f, 1}, {0.92f, 0.70f, 0.24f, 1}, {0.76f, 0.19f, 0.11f, 1}};
    case Game::MGS3: return {{0.66f, 0.78f, 0.42f, 1}, {0.88f, 0.72f, 0.28f, 1}, {0.82f, 0.32f, 0.24f, 1}};
    case Game::MGS4: return {{0.55f, 0.78f, 0.82f, 1}, {0.90f, 0.72f, 0.28f, 1}, {0.88f, 0.30f, 0.24f, 1}};
    case Game::MGSPW: return {{0.76f, 0.80f, 0.66f, 1}, {0.85f, 0.75f, 0.42f, 1}, {0.89f, 0.13f, 0.15f, 1}};
    }
    return {{0.42f, 0.90f, 0.45f, 1}, {1.0f, 0.82f, 0.25f, 1}, {0.95f, 0.35f, 0.35f, 1}};
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

void checklist(const char* id, const char* const* names, size_t count, uint64_t mask, int scroll)
{
    if (ImGui::BeginChild(id, ImVec2(0, 360), true)) {
        if (scroll != 0) {
            ImGui::SetScrollY(ImGui::GetScrollY() + scroll * ImGui::GetTextLineHeightWithSpacing() * 8);
        }
        for (size_t i = 0; i < count; ++i) {
            const bool done = (mask & (uint64_t{1} << i)) != 0;
            ImGui::TextColored(done ? ImVec4(0.42f, 0.90f, 0.45f, 1.0f)
                                    : ImVec4(1, 1, 1, 0.35f),
                               "%s  %s", done ? "x" : "-", names[i]);
        }
    }
    ImGui::EndChild();
}

void draw_mgs4_feats(const GameStats& stats, int scroll)
{
    const std::vector<codename::Match> matches = codename::all_matches_mgs4(stats);
    const auto matched = [&](const char* name) {
        for (const auto& match : matches) {
            if (std::strcmp(match.name, name) == 0) return true;
        }
        return false;
    };
    if (ImGui::BeginChild("mgs4_feats", ImVec2(0, 360), true)) {
        if (scroll != 0) {
            ImGui::SetScrollY(ImGui::GetScrollY()
                              + scroll * ImGui::GetTextLineHeightWithSpacing() * 8);
        }
        if (ImGui::BeginTable("mgs4_feat_rows", 2,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("emblem / requirement", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("progress", ImGuiTableColumnFlags_WidthFixed, 125.0f);
            const ImVec4 done_color(0.42f, 0.90f, 0.45f, 1.0f);
            const ImVec4 pending_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            const auto row = [&](const char* name, const char* goal, const char* value) {
                const bool done = matched(name);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextColored(done ? done_color : pending_color, "%s", name);
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextDisabled("%s", goal);
                ImGui::PopTextWrapPos();
                ImGui::TableNextColumn();
                ImGui::TextColored(done ? done_color : pending_color, "%s", value);
            };
            const auto count = [&](const char* name, const char* goal, int value, int target) {
                char text[40];
                snprintf(text, sizeof(text), "%d / %d", value, target);
                row(name, goal, text);
            };
            const auto time = [&](const char* name, const char* goal,
                                  double seconds, double target_seconds) {
                char current[16], target[16], text[40];
                format_time(seconds, current, sizeof(current));
                format_time(target_seconds, target, sizeof(target));
                snprintf(text, sizeof(text), "%s / %s", current, target);
                row(name, goal, text);
            };
            char text[96];
            count("BEAR", "100 CQC chokes", stats.cqc_chokes, 100);
            count("EAGLE", "150 headshots", stats.headshots, 150);
            snprintf(text, sizeof(text), "knife %d / 50\nCQC %d / 50\nalerts %d / 25",
                     stats.knife_defeats, stats.cqc_holds, stats.alerts);
            row("ASSASSIN", "50 knife, 50 CQC, max 25 alerts", text);
            snprintf(text, sizeof(text), "%d kills", stats.kills);
            row("PIGEON", "No kills", text);
            count("BLUE BIRD", "Give allies 50 items", stats.items_given, 50);
            count("HAWK", "Earn 25 ally praises", stats.praises, 25);
            count("LITTLE GRAY", "Acquire 69 weapons", stats.weapons_acquired, 69);
            count("ANT", "Search 50 held-up enemies", stats.body_searches, 50);
            count("GIBBON", "Hold up 50 enemies", stats.hold_ups, 50);
            time("TORTOISE", "60 min in box or drum", stats.box_time_seconds, 60 * 60);
            count("RABBIT", "Turn 100 magazine pages", stats.magazine_pages, 100);
            count("BEE", "50 Syringe / Scanning Plug uses", stats.syringe_uses, 50);
            time("GECKO", "60 min against walls", stats.wall_time_seconds, 60 * 60);
            count("SCARAB", "100 prone side rolls", stats.side_rolls, 100);
            count("FROG", "200 forward rolls", stats.forward_rolls, 200);
            time("INCH WORM", "Crawl for 60 min", stats.crawl_time_seconds, 60 * 60);
            time("LOBSTER", "Crouch for 150 min", stats.crouch_time_seconds, 150 * 60);
            count("HYENA", "Pick up 400 weapons / items", stats.pickups, 400);
            count("HOG", "Enter Combat High 10 times", stats.combat_highs, 10);
            count("PIG", "Use 40 recovery items", stats.rations_used, 40);
            count("COW", "Trigger 100 alerts", stats.alerts, 100);
            count("CROCODILE", "Kill 400 enemies", stats.kills, 400);
            time("GIANT PANDA", "Play for 30 hours", stats.play_time_seconds, 30 * 60 * 60);
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
    "01 Dremuchij South (VM)", "02 Dremuchij Swampland (VM)",
    "03 Dremuchij North (VM)", "04 Dolinovodno (VM)", "05 Rassvet (VM)",
    "06 Dremuchij East", "07 Rassvet", "08 Dolinovodno", "09 Dremuchij North",
    "10 Dremuchij Swampland", "11 Dremuchij South", "12 Chyornyj Prud",
    "13 Bolshaya Past South", "14 Bolshaya Past Base", "15 Bolshaya Past Crevice",
    "16 Chyornaya Peschera Cave Branch", "17 Chyornaya Peschera Cave",
    "18 Chyornaya Peschera Cave Entrance", "19 Ponizovje South", "20 Ponizovje West",
    "21 Ponizovje Warehouse Exterior", "22 Ponizovje Warehouse 1F",
    "23 Graniny Gorki South", "24 Graniny Gorki Lab Exterior Perimeter",
    "25 Graniny Gorki Lab Exterior Yard", "26 Graniny Gorki Lab 1F",
    "27 Graniny Gorki Lab B1 East", "28 Graniny Gorki Lab B1 West",
    "29 Svyatogornyj South", "30 Svyatogornyj West", "31 Svyatogornyj East",
    "32 Sokrovenno South", "33 Sokrovenno West", "34 Sokrovenno North",
    "35 Krasnogorje Tunnel", "36 Krasnogorje Mountain Base",
    "37 Krasnogorje Mountainside", "38 Krasnogorje Mountaintop",
    "39 Krasnogorje Mountaintop Behind Ruins", "40 Krasnogorje Mountaintop Ruins",
    "41 Groznyj Grad Underground Tunnel", "42 Groznyj Grad Southwest",
    "43 Groznyj Grad Northwest", "44 Groznyj Grad Northeast",
    "45 Groznyj Grad Southeast", "46 Groznyj Grad Holding Facility",
    "47 Weapons Lab East Wing 2F", "48 Weapons Lab West Wing 2F Corridor",
    "49 Tikhogornyj", "50 Tikhogornyj Behind Waterfall",
    "51 Weapons Lab Main Wing 1F", "52 Groznyj Grad B1F",
    "53 Groznyj Grad Escape", "54 Groznyj Grad Runway South",
    "55 Groznyj Grad Runway", "56 Groznyj Grad Runway After WIG",
    "57 Rail Bridge C3", "58 Rail Bridge Shagohod", "59 Rail Bridge North",
    "60 Lazorevo South", "61 Lazorevo North", "62 Zaozyorje West",
    "63 Zaozyorje East", "64 Rokovoj Bereg",
};

static_assert(std::size(kMgs3Captures) == 48);
static_assert(std::size(kMgs3Kerotans) == 64);

int mgs3_area_kerotan(const char* code)
{
    static constexpr AreaName kAreas[] = {
        {"v001a", "0"}, {"v003a", "1"}, {"v004a", "2"}, {"v005a", "3"},
        {"v006a", "4"}, {"v006b", "4"}, {"s002a", "5"}, {"s006a", "6"},
        {"s006b", "6"}, {"s005a", "7"}, {"s004a", "8"}, {"s003a", "9"},
        {"s001a", "10"}, {"s012a", "11"}, {"s021a", "12"}, {"s022a", "13"},
        {"s023a", "14"}, {"s031a", "15"}, {"s032a", "16"}, {"s032b", "16"},
        {"s033a", "17"}, {"s041a", "18"}, {"s042a", "19"}, {"s043a", "20"},
        {"s044a", "21"}, {"s051a", "22"}, {"s052a", "23"}, {"s052b", "24"},
        {"s053a", "25"}, {"s055a", "26"}, {"s056a", "27"}, {"s045a", "28"},
        {"s061a", "29"}, {"s062a", "30"}, {"s063a", "31"}, {"s063b", "31"},
        {"s064a", "32"}, {"s064b", "32"}, {"s065a", "33"}, {"s065b", "33"},
        {"s066a", "34"}, {"s071a", "35"}, {"s072a", "36"}, {"s072b", "36"},
        {"s073a", "37"}, {"s073b", "37"}, {"s075a", "38"}, {"s074a", "39"},
        {"s081a", "40"}, {"s091a", "41"}, {"s091b", "41"}, {"s091c", "41"},
        {"s092a", "42"}, {"s092b", "42"}, {"s092c", "42"}, {"s093a", "43"},
        {"s093b", "43"}, {"s093c", "43"}, {"s094a", "44"}, {"s094b", "44"},
        {"s094c", "44"}, {"s112a", "45"}, {"s101a", "46"}, {"s101b", "46"},
        {"s111a", "47"}, {"s151a", "48"}, {"s152a", "49"}, {"s121a", "50"},
        {"s122a", "51"}, {"s161a", "52"}, {"s162a", "53"}, {"s163a", "54"},
        {"s163b", "55"}, {"s171a", "56"}, {"s171b", "57"}, {"s181a", "58"},
        {"s182a", "59"}, {"s183a", "60"}, {"s191a", "61"}, {"s191b", "61"},
        {"s192a", "62"}, {"s201a", "63"},
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
};

constexpr RequirementsFn kRequirementsFns[] = {
    [static_cast<int>(Game::MG1)] = codename::elite_requirements_mg1,
    [static_cast<int>(Game::MG2)] = codename::elite_requirements_mg2,
    [static_cast<int>(Game::MGS1)] = codename::elite_requirements_mgs1,
    [static_cast<int>(Game::MGS2)] = codename::elite_requirements_mgs2,
    [static_cast<int>(Game::MGS3)] = codename::elite_requirements_mgs3,
    [static_cast<int>(Game::MGS4)] = codename::elite_requirements_mgs4,
    [static_cast<int>(Game::MGSPW)] = codename::elite_requirements_mgspw,
};

static_assert(std::size(kEvaluateFns) == 7);
static_assert(std::size(kRequirementsFns) == 7);

void draw_mgspw_current(const GameStats& stats)
{
    // Current sortie: segment deltas land at results tally (actions) or
    // lobby exit (heroism/XP/GMP); the master clock ticks live.
    const double seg_seconds = stats.seg_time_seconds;
    const unsigned long long seg_ms =
        static_cast<unsigned long long>(seg_seconds * 1000.0);
    char seg_clock[32];
    snprintf(seg_clock, sizeof(seg_clock), "%llu:%02llu.%03llu", seg_ms / 60000,
             (seg_ms / 1000) % 60, seg_ms % 1000);
    if (stats.pw_mission_id > 0) {
        char head[64];
        const char* rank_names[] = {"S", "A", "B", "C"};
        const char* rank = stats.pw_cur_rank >= 0 && stats.pw_cur_rank < 4
            ? rank_names[stats.pw_cur_rank] : "-";
        if (stats.pw_cur_best) {
            const unsigned long long best_ms =
                static_cast<unsigned long long>(stats.pw_cur_best) * 1000ULL / 300ULL;
            // Rank and time are separate bests and may come from different
            // runs, so they are labelled apart rather than read as one result.
            snprintf(head, sizeof(head), "mission %d  best rank %s  best time %llu:%02llu.%03llu",
                     stats.pw_mission_id, rank, best_ms / 60000, (best_ms / 1000) % 60,
                     best_ms % 1000);
        } else {
            snprintf(head, sizeof(head), "mission %d  first clear", stats.pw_mission_id);
        }
        ImGui::TextDisabled("%s", head);
    }
    ImGui::TextDisabled("%s", stats.seg_stage[0] ? stats.seg_stage : "-");
    ImGui::SetWindowFontScale(2.0f);
    ImGui::TextUnformatted(seg_clock);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::Spacing();
    // pw_mission_raw is the high-res TOTAL play clock (raw/300 ~= total);
    // the seg line above is the current mission time.
    char buf[64];
    if (ImGui::BeginTable("pw_current", 2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("field", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, 190.0f);
        const auto row = [&](const char* k, const char* v) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(k);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(v);
        };
        // The game keeps its own per-mission tally in each stat descriptor
        // (+0x18); it beats the client-side segment delta because the game
        // clears it at mission start. Fall back to the segment when a
        // descriptor is unresolved.
        const auto stat_row = [&](const char* k, int mission_value, int seg) {
            if (mission_value >= 0) {
                snprintf(buf, sizeof(buf), "%d", mission_value);
            } else {
                snprintf(buf, sizeof(buf), "%+d (seg)", seg);
            }
            row(k, buf);
        };
        stat_row("headshots", stats.pw_m_headshots, stats.seg_headshots);
        stat_row("kills", stats.pw_m_kills, stats.seg_kills);
        stat_row("tranq", stats.pw_m_tranq, stats.seg_tranq);
        if (stats.pw_m_alerts >= 0) {
            snprintf(buf, sizeof(buf), "%d", stats.pw_m_alerts);
            row("alerts", buf);
        }
        snprintf(buf, sizeof(buf), "%+d", stats.seg_heroism);
        row("heroism", buf);
        const double best_seconds = stats.pw_last_best_a / 300.0;
        const unsigned long long best_ms =
            static_cast<unsigned long long>(best_seconds * 1000.0);
        snprintf(buf, sizeof(buf), "%llu:%02llu.%03llu", best_ms / 60000,
                 (best_ms / 1000) % 60, best_ms % 1000);
        row("last best", buf);
        snprintf(buf, sizeof(buf), "%d%% (wpn %d)", stats.pw_player_hp * 100 / 8000,
                 stats.pw_weapon_id);
        row("player HP", buf);
        ImGui::EndTable();
    }
}

void draw_mgspw_global(const GameStats& stats)
{
    // Career totals. Clocks render from validated units; raw values live
    // under Forensics.
    char total_clock[32], stage_clock[32];
    snprintf(total_clock, sizeof(total_clock), "%u:%02u:%02u", stats.pw_total_play / 3600,
             (stats.pw_total_play / 60) % 60, stats.pw_total_play % 60);
    const unsigned long long stage_ms =
        static_cast<unsigned long long>(stats.pw_stage_play) * 1000ULL / 300ULL;
    snprintf(stage_clock, sizeof(stage_clock), "%llu:%02llu.%03llu", stage_ms / 60000,
             (stage_ms / 1000) % 60, stage_ms % 1000);
    if (ImGui::BeginTable("pw_global", 2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("field", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, 190.0f);
        const auto row = [&](const char* k, const char* v) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(k);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(v);
        };
        char buf[64];
        snprintf(buf, sizeof(buf), "%d (%+d last)", stats.pw_heroism,
                 stats.pw_heroism_delta);
        row("heroism", buf);
        snprintf(buf, sizeof(buf), "%u", stats.pw_gmp);
        row("GMP", buf);
        if (stats.pw_clears >= 0) {
            snprintf(buf, sizeof(buf), "%d", stats.pw_clears);
            row("clears", buf);
        }
        if (stats.pw_unique_cleared >= 0) {
            snprintf(buf, sizeof(buf), "%d (S %d)", stats.pw_unique_cleared,
                     stats.pw_s_missions);
            row("missions cleared", buf);
        }
        snprintf(buf, sizeof(buf), "%d lethal / %d non-lethal", stats.pw_kills,
                 stats.pw_tranq);
        row("takedowns", buf);
        if (stats.pw_damage_taken >= 0) {
            // 8000 is a full health bar, so this reads as "bars of damage".
            snprintf(buf, sizeof(buf), "%d  (%.1f bars)", stats.pw_damage_taken,
                     stats.pw_damage_taken / 8000.0);
            row("damage taken", buf);
        }
        if (stats.pw_stealth_kills >= 0 && stats.pw_kills >= 0) {
            snprintf(buf, sizeof(buf), "%d of %d kills", stats.pw_stealth_kills,
                     stats.pw_kills);
            row("unseen", buf);
        }
        if (stats.pw_pistol_takedowns >= 0) {
            snprintf(buf, sizeof(buf),
                     "pistol %d+%d  AR %d  SR %d+%d  LMG %d  SG %d  CQC %d",
                     stats.pw_pistol_takedowns, stats.pw_pistol_lethal,
                     stats.pw_ar_takedowns, stats.pw_sniper_takedowns,
                     stats.pw_sniper_nonlethal, stats.pw_lmg_takedowns,
                     stats.pw_shotgun_takedowns, stats.pw_cqc_takedowns);
            snprintf(buf, sizeof(buf), "grenade %d  rocket %d  placed %d",
                     stats.pw_grenade_takedowns, stats.pw_rocket_takedowns,
                     stats.pw_placed_takedowns);
            row("explosives", buf);
            row("by weapon", buf);
        }
        snprintf(buf, sizeof(buf), "HS %d  AL %d", stats.pw_headshots, stats.pw_alerts);
        row("lifetime", buf);
        if (stats.pw_body_kills >= 0 && stats.pw_kills >= 0 && stats.pw_headshots >= 0
            && stats.pw_grenade_takedowns >= 0 && stats.pw_rocket_takedowns >= 0) {
            // Headshots are stored as one total and non-headshot kills
            // separately, so the lethal/tranq split is a subtraction. Explosive
            // kills carry no hit location and count as neither, so they have to
            // come out too - a rocket run moved kills by 3 while both the
            // headshot and body counters stayed put.
            const int explosive = stats.pw_grenade_takedowns + stats.pw_rocket_takedowns;
            const int lethal_hs = stats.pw_kills - stats.pw_body_kills - explosive;
            snprintf(buf, sizeof(buf), "%d lethal / %d tranq", lethal_hs,
                     stats.pw_headshots - lethal_hs);
            row("headshots", buf);
        }
        if (stats.pw_fulton_recoveries >= 0) {
            snprintf(buf, sizeof(buf), "%d  (+%d POW)", stats.pw_fulton_recoveries,
                     stats.pw_prisoner_extractions);
            row("Fulton", buf);
        }
        if (stats.pw_nokill_clears >= 0) {
            snprintf(buf, sizeof(buf), "%d no-kill  %d no-alert  %d no-item",
                     stats.pw_nokill_clears, stats.pw_noalert_clears,
                     stats.pw_noitem_clears);
            row("clean clears", buf);
            snprintf(buf, sizeof(buf), "%d", stats.pw_holdups);
            row("hold-ups", buf);
        }
        if (stats.pw_headshots >= 0 && stats.pw_kills >= 0 && stats.pw_tranq >= 0) {
            // Draft lethal/non-lethal score: sleep+stun+incap - 2*kills,
            // with tranq takedowns as the combined non-lethal input.
            // Provisional until stun/incap split out.
            snprintf(buf, sizeof(buf), "%d", stats.pw_tranq - 2 * stats.pw_kills);
            row("lethal score (prov.)", buf);
        }
        row("total play", total_clock);
        row("stage play", stage_clock);
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::TextDisabled("weapon XP (settles at mission end)");
    bool any_weapon = false;
    for (int i = 0; i < 16; ++i) {
        if (stats.pw_weapon_use[i] > 0) {
            any_weapon = true;
            break;
        }
    }
    if (any_weapon && ImGui::BeginTable("pw_weap", 4, ImGuiTableFlags_RowBg)) {
        for (int i = 0; i < 16; ++i) {
            if (stats.pw_weapon_use[i] <= 0) {
                continue;
            }
            ImGui::TableNextColumn();
            ImGui::Text("id%02d %d", i + 1, stats.pw_weapon_use[i]);
        }
        ImGui::EndTable();
    } else if (!any_weapon) {
        ImGui::TextDisabled("no weapon XP yet");
    }
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Forensics")) {
        const uint64_t raw = stats.pw_mission_raw;
        static uint64_t last_raw = 0;
        static uint64_t last_tick = 0;
        static char rate[48] = "measuring...";
        const uint64_t tick = GetTickCount64();
        if (last_tick && tick > last_tick && raw != last_raw) {
            const double r =
                (raw >= last_raw ? static_cast<double>(raw - last_raw)
                                 : -static_cast<double>(last_raw - raw))
                * 1000.0 / static_cast<double>(tick - last_tick);
            snprintf(rate, sizeof(rate), "~%.0f/s (nominal 300)", r);
            last_raw = raw;
            last_tick = tick;
        } else if (!last_tick) {
            last_raw = raw;
            last_tick = tick;
        }
        char hex[32], dec[32], aux[32], area[32], resolvers[32], f130[32];
        snprintf(hex, sizeof(hex), "0x%llX", static_cast<unsigned long long>(raw));
        snprintf(dec, sizeof(dec), "%llu", static_cast<unsigned long long>(raw));
        snprintf(aux, sizeof(aux), "0x%llX (%llu)",
                 static_cast<unsigned long long>(stats.pw_mission_aux),
                 static_cast<unsigned long long>(stats.pw_mission_aux));
        snprintf(area, sizeof(area), "%u / %u", stats.pw_area, stats.pw_mission_secondary);
        snprintf(resolvers, sizeof(resolvers), "%s/%s/%s",
                 stats.pw_saveroot_ok ? "SAVE" : "-save",
                 stats.pw_mission_ok ? "TIME" : "-time",
                 stats.pw_chararray_ok ? "CHAR" : "-char");
        if (stats.pw_fulton >= 0) {
            snprintf(f130, sizeof(f130), "%d (?)", stats.pw_fulton);
        } else {
            snprintf(f130, sizeof(f130), "-");
        }
        char fulton_str[32];
        if (stats.pw_fulton_recoveries >= 0) {
            snprintf(fulton_str, sizeof(fulton_str), "%d (%+d sortie)", stats.pw_fulton_recoveries,
                     stats.seg_fulton);
        } else {
            snprintf(fulton_str, sizeof(fulton_str), "-");
        }
        if (ImGui::BeginTable("pw_forensic", 2,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("field", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, 190.0f);
            const auto row = [&](const char* k, const char* v) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(k);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(v);
            };
            row("total raw dec", dec);
            row("total raw hex", hex);
            row("mission tick rate", rate);
            row("aux [+0x08]", aux);
            row("area [+0x10] / sec [+0x14]", area);
            row("stage", stats.pw_stage[0] ? stats.pw_stage : "-");
            row("fulton/XP-next? [+0x130]", f130);
            row("Fulton [0x2008E]", fulton_str);
            char twin[32];
            snprintf(twin, sizeof(twin), "%u", stats.pw_last_score);
            row("twin [+0x278]", twin);
            row("resolvers", resolvers);
            ImGui::EndTable();
        }
    }
}

void draw_mgspw_codename(const GameStats& stats)
{
    // Same grammar as the Summary tab: the title, then the axes it is scored
    // on. PW classifies career play rather than a single run, so every row is
    // a lifetime counter.
    const auto [id_green, id_yellow, id_red] = id_colors(Game::MGSPW);
    const auto match = codename::evaluate_mgspw(stats);
    const ImVec4 title_color = !match ? ImVec4(1, 1, 1, 0.35f)
        : std::strcmp(match->name, "FOXHOUND") == 0 || std::strcmp(match->name, "FOX") == 0
            ? id_green
            : ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImGui::SetWindowFontScale(2.0f);
    ImGui::TextColored(title_color, "%s", match ? match->name : "---");
    ImGui::SetWindowFontScale(1.0f);
    const codename::PwGrade grade = codename::pw_grade(stats);
    if (match) {
        if (grade.grade > 0) {
            ImGui::TextDisabled("grade %d", grade.grade);
        } else {
            ImGui::TextDisabled("no grade yet");
        }
        ImGui::SameLine();
        if (grade.blocker) {
            const bool ratio = std::strcmp(grade.blocker, "cooperation ratio") == 0;
            ImGui::TextDisabled("- grade %d needs %s %s%.*f", grade.next,
                                grade.blocker,
                                std::strncmp(grade.blocker, "camaraderie under", 17) == 0
                                    ? "<= " : ">= ",
                                ratio ? 2 : 0, grade.need);
        } else if (grade.grade == 5) {
            ImGui::TextDisabled("- highest grade");
        }
    }
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::BeginTable("pw_reqs", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("toward FOX / FOXHOUND", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        for (const codename::ReqStatus& r : codename::elite_requirements_mgspw(stats)) {
            char value[64];
            if (r.limit == 0) {
                snprintf(value, sizeof(value), "%.0f", r.current);
            } else {
                snprintf(value, sizeof(value), "%.0f / %.0f", r.current, r.limit);
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.label);
            ImGui::TableNextColumn();
            ImGui::TextColored(r.pass ? id_green : id_red, "%s", value);
        }
        // Context, not a requirement: the counters the axes are read from.
        const auto plain = [](const char* key, int value) {
            char buf[24];
            snprintf(buf, sizeof(buf), "%d", value);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", key);
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", buf);
        };
        plain("lethal kills", stats.pw_kills > 0 ? stats.pw_kills : 0);
        plain("CQC takedowns", stats.pw_cqc_takedowns > 0 ? stats.pw_cqc_takedowns : 0);
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("FOX and FOXHOUND share these axes; the co-op half is");
    ImGui::TextDisabled("not measured, so the solo name is shown.");

    // Ground truth: what the game has actually awarded, from save+0x1BFF0.
    if (!stats.pw_codename_state_ok) {
        return;
    }
    int owned = 0;
    for (int id = 1; id <= 24; ++id) {
        owned += stats.pw_codename_state[id] & 1;
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("awarded  %d / 24", owned);
    if (stats.pw_insignias >= 0) {
        ImGui::TextDisabled("insignias  %d / 110", stats.pw_insignias);
    }
    if (!owned) {
        ImGui::TextDisabled("none yet - codenames are granted at a result screen");
        return;
    }
    if (ImGui::BeginTable("pw_titles", 2, ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("codename", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("grade", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        for (int id = 1; id <= 24; ++id) {
            const uint8_t state = stats.pw_codename_state[id];
            if (!(state & 1)) {
                continue;
            }
            const int grade = (state >> 2) & 7;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(codename::pw_title_name(id));
            ImGui::TableNextColumn();
            ImGui::Text("%d", grade ? grade : 1);
        }
        ImGui::EndTable();
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
        next_poll = now + 100;
    }
    const char* panel_title = g_game == Game::MGS3 || g_game == Game::MGSPW
        ? "FOXHOUND tracker"
        : "BIG BOSS tracker";
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 16.0f,
                                  viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_FirstUseEver, ImVec2(0.0f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(380, 480), ImGuiCond_FirstUseEver);
    ImGui::Begin(panel_title, &g.show,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);

    if (!have_stats) {
        ImGui::TextDisabled("no active ranked run");
        ImGui::End();
        return;
    }

    static int selected_tab = 0;
    const int tab_count = g_game == Game::MGS3 ? 3 : g_game == Game::MGS4 ? 2
        : g_game == Game::MGSPW                                             ? 3
                                                                            : 0;
    if (tab_count && key_pressed(kTabKey)) {
        selected_tab = (selected_tab + 1) % tab_count;
    }
    const bool scroll_up = key_pressed(VK_UP);
    const bool scroll_down = key_pressed(VK_DOWN);
    const int scroll = scroll_up ? -1 : scroll_down ? 1 : 0;
    const bool tabs = tab_count && ImGui::BeginTabBar("tracker_tabs");
    if (g_game == Game::MGSPW) {
        if (!tabs || ImGui::BeginTabItem(
                         "Current", nullptr,
                         selected_tab == 0 ? ImGuiTabItemFlags_SetSelected : 0)) {
            draw_mgspw_current(stats);
            if (tabs) {
                ImGui::EndTabItem();
            }
        }
        if (tabs && ImGui::BeginTabItem(
                        "Global", nullptr,
                        selected_tab == 1 ? ImGuiTabItemFlags_SetSelected : 0)) {
            draw_mgspw_global(stats);
            ImGui::EndTabItem();
        }
        if (tabs && ImGui::BeginTabItem(
                        "Codename", nullptr,
                        selected_tab == 2 ? ImGuiTabItemFlags_SetSelected : 0)) {
            draw_mgspw_codename(stats);
            ImGui::EndTabItem();
        }
        if (tabs) {
            ImGui::EndTabBar();
        }
        ImGui::End();
        return;
    }
    (void)scroll;
    const bool summary = !tabs || ImGui::BeginTabItem(
        "Summary", nullptr, selected_tab == 0 ? ImGuiTabItemFlags_SetSelected : 0);
    if (summary) {
    const auto eval_fn = kEvaluateFns[static_cast<int>(g_game)];
    auto match = eval_fn ? eval_fn(stats) : std::optional<codename::Match>{};

    const auto [id_green, id_yellow, id_red] = id_colors(g_game);
    const ImVec4 codename_color = !match ? ImVec4(1, 1, 1, 0.35f)
        : std::strcmp(match->name, "FOX") == 0 ? id_yellow
        : std::strcmp(match->name, "BIG BOSS") == 0 || std::strcmp(match->name, "FOXHOUND") == 0
            ? id_green : match->kind == codename::Kind::Worst ? id_red
            : ImGui::GetStyleColorVec4(ImGuiCol_Text);
    ImGui::SetWindowFontScale(2.0f);
    ImGui::TextColored(codename_color, "%s", match ? match->name : "---");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Separator();
    ImGui::Spacing();

    const ImVec4 green(0.42f, 0.90f, 0.45f, 1.0f);
    if (ImGui::BeginTable("reqs", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("requirement", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 120.0f);

        if (!stats.mgs1_japanese_original) {
            const bool classic_mg = g_game == Game::MG1 || g_game == Game::MG2;
            const bool known_difficulty = classic_mg
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
            ImGui::TextColored(valid_difficulty ? green : ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
                               "%s%s", classic_mg
                                   ? (stats.difficulty == Difficulty::Extreme ? "Original" : "Easy")
                                   : difficulty_name(stats.difficulty),
                               known_difficulty ? "" : " (?)");
        }

        std::vector<codename::ReqStatus> reqs;
        if (const auto req_fn = kRequirementsFns[static_cast<int>(g_game)]) {
            reqs = req_fn(stats);
        }
        for (const codename::ReqStatus& r : reqs) {
            char ratio[96];
            if (std::strcmp(r.label, "special items") == 0 && g_game == Game::MGS2) {
                ratio[0] = '\0';
                const uint16_t used = stats.special_items_mask & 0x000F;
                const char* names[] = {
                    "Stealth Camo", "Infinity Bandana/Wig", "O2 Wig", "Grip Wig"};
                for (int i = 0; i < 4; ++i) {
                    if ((used & (1u << i)) != 0) {
                        snprintf(ratio + strlen(ratio), sizeof(ratio) - strlen(ratio), "%s%s",
                                 ratio[0] ? ", " : "", names[i]);
                    }
                }
                if (!used) {
                    snprintf(ratio, sizeof(ratio), "NONE");
                }
            } else if (std::strcmp(r.label, "special items") == 0 && g_game == Game::MGS3) {
                ratio[0] = '\0';
                const uint16_t used = stats.special_items_mask & 0x07;
                const char* names[] = {"Stealth Camo", "Infinity Face Paint", "EZ Gun"};
                for (int i = 0; i < 3; ++i) {
                    if ((used & (1u << i)) != 0) {
                        snprintf(ratio + strlen(ratio), sizeof(ratio) - strlen(ratio), "%s%s",
                                 ratio[0] ? ", " : "", names[i]);
                    }
                }
                if (!used) {
                    snprintf(ratio, sizeof(ratio), "NONE");
                }
            } else if (std::strcmp(r.label, "special items") == 0
                       && (g_game == Game::MG1 || g_game == Game::MG2)) {
                snprintf(ratio, sizeof(ratio), "%s",
                         stats.special_item_used ? "Infinity Bandana" : "NONE");
            } else if (std::strcmp(r.label, "special items") == 0
                       && g_game == Game::MGS4) {
                snprintf(ratio, sizeof(ratio), "%s",
                         stats.special_item_used ? "USED" : "NONE");
            } else if (std::strcmp(r.label, "radar") == 0 && g_game == Game::MGS1) {
                snprintf(ratio, sizeof(ratio), "%s", stats.radar_off ? "OFF" : "ON");
            } else if (std::strcmp(r.label, "radar") == 0 && g_game == Game::MGS2) {
                const char* type = stats.radar_type == 0    ? "TYPE-A"
                                   : stats.radar_type == 0x20 ? "TYPE-B"
                                   : stats.radar_type == 4    ? "OFF"
                                                              : "?";
                snprintf(ratio, sizeof(ratio), "%s", type);
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
            default:
                if (r.limit == 0) {
                    snprintf(ratio, sizeof(ratio), "%.0f", r.current);
                } else {
                    snprintf(ratio, sizeof(ratio), "%.0f / %.0f", r.current, r.limit);
                }
                break;
            }

            const bool radar_invalid = std::strcmp(r.label, "radar") == 0
                && g_game == Game::MGS2 && stats.radar_type != 4;
            const bool over = !r.pass || radar_invalid;
            const auto op = static_cast<codename::Op>(r.op);
            const bool near_limit = !over && (op == codename::Op::Le || op == codename::Op::Lt)
                && r.limit != 0 && r.current >= r.limit * 0.75;
            const ImVec4 state_col = over ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f)
                                          : near_limit ? ImVec4(1.0f, 0.82f, 0.25f, 1.0f)
                                                       : ImVec4(0.4f, 0.9f, 0.5f, 1.0f);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.label);
            ImGui::TableNextColumn();
            ImGui::TextColored(state_col, "%s", ratio);
        }

        auto plain_row = [&](const char* key, const char* val) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", key);
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", val);
        };

        auto plain_count = [&](const char* key, int value) {
            char buf[24];
            snprintf(buf, sizeof(buf), "%d", value);
            plain_row(key, buf);
        };

        auto plain_pair = [&](const char* key, int value, int maximum) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d / %d", value, maximum);
            plain_row(key, buf);
        };

        if (g_game == Game::MGS1) {
            plain_count("saves", stats.saves);
            plain_pair("health", stats.current_health, stats.max_health);
            if (stats.diazepam_frames > 0) {
                char buf[24];
                snprintf(buf, sizeof(buf), "%.1fs", stats.diazepam_frames / 30.0);
                plain_row("diazepam", buf);
            }
        } else if (g_game == Game::MGS2) {
            plain_pair("health", stats.current_health, stats.max_health);
            plain_row("sea louse", stats.sea_louse ? "YES" : "NO");
            plain_count("clearing escapes", stats.clearing_escapes);
            plain_count("times seen", stats.times_seen);
            plain_count("mechs destroyed", stats.mechs_destroyed);
            plain_count("pull-ups", stats.pull_ups);
            plain_row("alert state", stats.alert_state_available
                                         ? alert_state_name(stats.alert_state)
                                         : "unavailable");
        } else if (g_game == Game::MGS3) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%d / 48", stats.plants_captured);
            plain_row("captures", buf);
            const int current_kerotan = mgs3_area_kerotan(stats.area_code);
            snprintf(buf, sizeof(buf), "%d / 64  %s", stats.kerotans,
                     current_kerotan >= 0
                             && (stats.kerotan_mask & (uint64_t{1} << current_kerotan)) != 0
                         ? "x" : "-");
            plain_row("kerotans", buf);
            plain_count("meals eaten", stats.meals_eaten);
        } else if (g_game == Game::MGS4) {
            plain_count("flashbacks", stats.flashbacks_viewed);
        }
        ImGui::EndTable();
    }

    if (stats.area_code[0]) {
        char buf[96];
        const char* area = area_name(g_game, stats.area_code);
        snprintf(buf, sizeof(buf), area ? "%s (%s)" : "%s", area ? area : stats.area_code,
                 stats.area_code);
        ImGui::Spacing();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", buf);
        ImGui::PopTextWrapPos();
    }

    if (tabs) {
        ImGui::EndTabItem();
    }
    }

    if (tabs && g_game == Game::MGS4 && ImGui::BeginTabItem(
                    "Feats", nullptr, selected_tab == 1 ? ImGuiTabItemFlags_SetSelected : 0)) {
        draw_mgs4_feats(stats, scroll);
        ImGui::EndTabItem();
    }
    if (tabs && g_game == Game::MGS3 && ImGui::BeginTabItem(
                    "Capture", nullptr, selected_tab == 1 ? ImGuiTabItemFlags_SetSelected : 0)) {
        ImGui::Text("%d / 48", stats.plants_captured);
        checklist("captures", kMgs3Captures, std::size(kMgs3Captures), stats.capture_mask, scroll);
        ImGui::EndTabItem();
    }
    if (tabs && g_game == Game::MGS3 && ImGui::BeginTabItem(
                    "Kerotan", nullptr, selected_tab == 2 ? ImGuiTabItemFlags_SetSelected : 0)) {
        ImGui::Text("%d / 64", stats.kerotans);
        checklist("kerotans", kMgs3Kerotans, std::size(kMgs3Kerotans), stats.kerotan_mask, scroll);
        ImGui::EndTabItem();
    }
    if (tabs) {
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void STDMETHODCALLTYPE hk_execute_command_lists(ID3D12CommandQueue* queue, UINT count,
                                                 ID3D12CommandList* const* lists)
{
    if (!g.queue12 && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
        queue->AddRef();
        if (InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(&g.queue12), queue,
                                              nullptr)) {
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
    wait_d3d12(&frame);
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
    g.queue12->ExecuteCommandLists(1, lists);
    const UINT64 fence = ++g.next_fence12;
    if (FAILED(g.queue12->Signal(g.fence12, fence))) return false;
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
    if (MH_Initialize() != MH_OK) {
        LOG_ERROR("MH_Initialize failed");
        return false;
    }
    if (MH_CreateHook(present_target, reinterpret_cast<void*>(&hk_present),
                      reinterpret_cast<void**>(&oPresent)) != MH_OK
        || MH_CreateHook(resize_target, reinterpret_cast<void*>(&hk_resize_buffers),
                         reinterpret_cast<void**>(&oResizeBuffers)) != MH_OK
        || (execute_target
            && MH_CreateHook(execute_target, reinterpret_cast<void*>(&hk_execute_command_lists),
                             reinterpret_cast<void**>(&oExecuteCommandLists)) != MH_OK)) {
        LOG_ERROR("MH_CreateHook failed");
        return false;
    }
    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        LOG_ERROR("MH_EnableHook failed");
        return false;
    }
    return true;
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

void start_overlay(const char* game_label, StatsFn stats_fn, const wchar_t* game_module, Game game)
{
    g_label = game_label;
    g_stats_fn = stats_fn;
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
