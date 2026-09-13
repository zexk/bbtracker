#include "overlay.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <misc/freetype/imgui_freetype.h>

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
#include "../games/mgs3/checklists.h"
#include "../games/mgs4/flashbacks.h"
#include "../games/mgspw/names.h"

// Embedded from the Inter release pinned in CMakeLists.txt.
extern const unsigned char inter_regular[];
extern const unsigned int inter_regular_size;
extern const unsigned char inter_semibold[];
extern const unsigned int inter_semibold_size;
extern const unsigned char inter_bold[];
extern const unsigned int inter_bold_size;

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
ImFont* g_font_semibold = nullptr;
ImFont* g_font_bold = nullptr;
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
constexpr float kFourKHeight = 2160.0f;
constexpr float kFourKScale = 2.0f;
constexpr float kBaseFontSize = 14.0f;
constexpr DWORD kStatsPollMs = 100;
constexpr DWORD kModuleRetryMs = 500;
constexpr DWORD kHookRetryMs = 5000;
constexpr unsigned kProbeTimingSampleCount = 240;
constexpr uint64_t kMicrosecondsPerSecond = 1000000;
constexpr int kDummyWindowSize = 100;

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
    const float resolution_scale = render_height >= kFourKHeight ? kFourKScale : 1.0f;
    return dpi_scale > resolution_scale ? dpi_scale : resolution_scale;
}

static_assert(overlay_scale(1080.0f, 1.0f) == 1.0f);
static_assert(overlay_scale(kFourKHeight, 1.0f) == kFourKScale);
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
    ImFontConfig font_cfg;
    font_cfg.FontDataOwnedByAtlas = false;
    // Light auto-hinting: vertical stem snapping without distorting advances.
    font_cfg.FontLoaderFlags = ImGuiFreeTypeLoaderFlags_LightHinting;
    io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(inter_regular),
                                   static_cast<int>(inter_regular_size), kBaseFontSize, &font_cfg);
    g_font_semibold = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(inter_semibold),
                                                     static_cast<int>(inter_semibold_size), kBaseFontSize,
                                                     &font_cfg);
    g_font_bold = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(inter_bold),
                                                 static_cast<int>(inter_bold_size), kBaseFontSize,
                                                 &font_cfg);
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

using ThemeFn = void (*)(ImGuiStyle&);

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

const char* exact_area_name(const char* code, std::span<const AreaName> areas)
{
    for (const AreaName& area : areas) {
        if (std::strcmp(code, area.code) == 0) {
            return area.name;
        }
    }
    return nullptr;
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
constexpr int kSecondsPerMinute = 60;
constexpr int kSecondsPerHour = kSecondsPerMinute * kSecondsPerMinute;

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

IdColors id_colors(Game game);

// Dimmed text for a value the probe has not resolved, or an entry not yet
// earned. Each game theme sets its own TextDisabled.
ImVec4 unset_color()
{
    return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
}

ImVec4 match_color_default(const codename::Match& match, const IdColors& colors)
{
    if (std::strcmp(match.name, "FOX") == 0) return colors.yellow;
    if (std::strcmp(match.name, "BIG BOSS") == 0
        || std::strcmp(match.name, "FOXHOUND") == 0) return colors.green;
    if (match.kind == codename::Kind::Worst) return colors.red;
    return ImGui::GetStyleColorVec4(ImGuiCol_Text);
}

// Dynamic fonts allow any size; panel ranks scale from the base so they track
// DPI and 4K scaling in apply_ui_scale().
constexpr float kTitleScale = 1.7f;
constexpr float kSectionScale = 1.15f;
constexpr float kNoteScale = 0.88f;

void push_font(ImFont* font, float scale)
{
    ImGui::PushFont(font, ImGui::GetStyle().FontSizeBase * scale);
}

void table_headers_row()
{
    push_font(g_font_semibold, 1.0f);
    ImGui::TableHeadersRow();
    ImGui::PopFont();
}

void section_text(const char* text)
{
    push_font(g_font_semibold, kSectionScale);
    ImGui::SeparatorText(text);
    ImGui::PopFont();
}

// Thousands as "50k" / "12.8k". The PW grade gates run to six figures, which
// is more digits than the columns they sit in can carry.
void format_thousands(double value, char* buf, size_t len)
{
    constexpr double kThousand = 1000.0;
    constexpr double kWholeThousandsAt = 100000.0;
    if (value < kThousand) {
        snprintf(buf, len, "%.0f", value);
    } else if (std::fmod(value, kThousand) == 0.0 || value >= kWholeThousandsAt) {
        snprintf(buf, len, "%.0fk", value / kThousand);
    } else {
        snprintf(buf, len, "%.1fk", value / kThousand);
    }
}

// Up/Down move a scrolling panel by eight lines a press.
void apply_scroll(int scroll)
{
    constexpr int kScrollLinesPerPress = 8;
    if (scroll != 0) {
        ImGui::SetScrollY(ImGui::GetScrollY()
                          + scroll * ImGui::GetTextLineHeightWithSpacing() * kScrollLinesPerPress);
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
    constexpr float kMaxChildHeight = 360.0f;
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f),
                                        ImVec2(FLT_MAX, ui_size(kMaxChildHeight)));
    return ImGui::BeginChild(id, ImVec2(0, 0),
                             ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);
}

void checklist(const char* id, const char* const* names, const uint8_t* bits, size_t count,
               uint64_t mask, int scroll)
{
    const ImVec4 done_color = id_colors(g_game).green;
    const ImVec4 todo_color = unset_color();
    if (begin_fitted_child(id)) {
        apply_scroll(scroll);
        for (size_t i = 0; i < count; ++i) {
            const size_t bit = bits ? bits[i] : i;
            const bool done = (mask & (uint64_t{1} << bit)) != 0;
            push_font(g_font_semibold, 1.0f);
            ImGui::TextColored(done ? done_color : todo_color, "%s", done ? "✓" : "✗");
            ImGui::PopFont();
            ImGui::SameLine();
            ImGui::TextColored(done ? done_color : todo_color, "%s", names[i]);
        }
    }
    ImGui::EndChild();
}

using EvaluateFn = std::optional<codename::Match> (*)(const GameStats&);
using RequirementsFn = std::vector<codename::ReqStatus> (*)(const GameStats&);

const char* area_name(Game game, const char* code);

struct DifficultyDisplay {
    const char* name;
    bool known;
    bool valid;
};

DifficultyDisplay difficulty_standard(const GameStats& stats)
{
    return {difficulty_name(stats.difficulty), stats.difficulty_game_byte % 10 == 0,
            stats.difficulty == Difficulty::Extreme
                || stats.difficulty == Difficulty::EuroExtreme};
}

void summary_count(const char* key, int value)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%d", value);
    dim_row(key, buf);
}

void summary_pair(const char* key, int value, int maximum)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d / %d", value, maximum);
    dim_row(key, buf);
}

void summary_inline_pair(const char* key, const char* left, int left_value,
                         const char* right, int right_value)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%s %d · %s %d", left, left_value, right, right_value);
    dim_row(key, buf);
}

using PanelTabDraw = void (*)(const GameStats&, int);
struct PanelTab {
    const char* title;
    PanelTabDraw draw;
};

// Keep game drawing beside its data; these share the small ImGui helpers above.
#include "games/mg12.inc"
#include "games/mgs1.inc"
#include "games/mgs2.inc"
#include "games/mgs3.inc"
#include "games/mgs4.inc"
#include "games/mgspw.inc"
#include "games/babel.inc"

struct GameOverlay {
    EvaluateFn evaluate;
    RequirementsFn requirements;
    std::span<const PanelTab> tabs;
    const char* title;
    ThemeFn theme;
    IdColors colors;
    ImVec4 (*match_color)(const codename::Match&, const IdColors&);
    const char* (*area_name)(const char*);
    std::span<const char* const> special_items;
    DifficultyDisplay (*difficulty)(const GameStats&);
    bool (*format_requirement)(const codename::ReqStatus&, const GameStats&, char*, size_t);
    void (*summary_extra)(const GameStats&);
    void (*summary_area)(const GameStats&, char*, size_t);
    std::vector<codename::Match> (*all_matches)(const GameStats&);
};

const GameOverlay& overlay_for(Game game);

void draw_summary(const GameStats& stats, int)
{
    const auto& match = g_eval.match;
    const GameOverlay& overlay = overlay_for(g_game);

    const auto [id_green, id_yellow, id_red] = overlay.colors;
    const ImVec4 codename_color = match ? overlay.match_color(*match, overlay.colors)
                                             : unset_color();
    push_font(g_font_semibold, kTitleScale);
    ImGui::TextColored(codename_color, "%s", match ? match->name : "---");
    ImGui::PopFont();

    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::BeginTable("reqs", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("requirement", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);

        if (!stats.mgs1_japanese_original) {
            const DifficultyDisplay difficulty = overlay.difficulty(stats);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("difficulty");
            ImGui::TableNextColumn();
            ImGui::TextColored(difficulty.known && difficulty.valid ? id_green : id_red,
                               "%s%s", difficulty.name, difficulty.known ? "" : " (?)");
        }

        const auto& reqs = g_eval.reqs;
        for (const codename::ReqStatus& r : reqs) {
            char ratio[96];
            if (overlay.format_requirement
                && overlay.format_requirement(r, stats, ratio, sizeof(ratio))) {
                // Game-specific labels are formatted by their own overlay.
            } else if (std::strcmp(r.label, "special items") == 0) {
                join_flags(ratio, sizeof(ratio), stats.special_items_mask,
                           overlay.special_items);
            } else switch (static_cast<codename::ReqFmt>(r.fmt)) {
            case codename::ReqFmt::Time: {
                char cur[16];
                format_time(r.current * kSecondsPerHour, cur, sizeof(cur));
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

        if (overlay.summary_extra) overlay.summary_extra(stats);
        ImGui::EndTable();
    }

    if (stats.area_code[0]) {
        char buf[128];
        const char* area = area_name(g_game, stats.area_code);
        snprintf(buf, sizeof(buf), area ? "%s (%s)" : "%s", area ? area : stats.area_code,
                 stats.area_code);
        if (overlay.summary_area) overlay.summary_area(stats, buf, sizeof(buf));
        ImGui::Spacing();
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", buf);
        ImGui::PopTextWrapPos();
    }

}

constexpr PanelTab kGenericTabs[] = {{"Summary", draw_summary}};
constexpr PanelTab kMgs2Tabs[] = {{"Summary", draw_summary}, {"Dog Tags", draw_mgs2_dog_tags}};
constexpr PanelTab kMgs3Tabs[] = {{"Summary", draw_summary},
                                  {"Capture", draw_mgs3_captures},
                                  {"Kerotan", draw_mgs3_kerotans}};
constexpr PanelTab kMgs4Tabs[] = {{"Summary", draw_summary},
                                  {"Feats", draw_mgs4_feats},
                                  {"Flashbacks", draw_mgs4_flashbacks}};
constexpr PanelTab kMgspwTabs[] = {{"Summary", draw_mgspw_summary},
                                   {"Career", draw_mgspw_global},
                                   {"Insignia", draw_mgspw_insignia},
                                   {"Codenames", draw_mgspw_codenames}};

// One row per Game, in enum order. Game-specific display choices stay here.
constexpr GameOverlay kOverlays[] = {
    {codename::evaluate_mg1, codename::elite_requirements_mg1, kGenericTabs,
     "BIG BOSS tracker", apply_theme_mg1, kMg1Colors, match_color_default, nullptr, {},
     difficulty_mg12, requirement_mg12, nullptr, nullptr, nullptr},
    {codename::evaluate_mg2, codename::elite_requirements_mg2, kGenericTabs,
     "BIG BOSS tracker", apply_theme_mg2, kMg2Colors, match_color_default, nullptr, {},
     difficulty_mg12, requirement_mg12, nullptr, nullptr, nullptr},
    {codename::evaluate_mgs1, codename::elite_requirements_mgs1, kGenericTabs,
     "BIG BOSS tracker", apply_theme_mgs1, kMgs1Colors, match_color_default,
     mgs1_area_name, {},
     difficulty_mgs1, requirement_mgs1, summary_extra_mgs1, nullptr, nullptr},
    {codename::evaluate_mgs2, codename::elite_requirements_mgs2, kMgs2Tabs,
     "BIG BOSS tracker", apply_theme_mgs2, kMgs2Colors, match_color_default,
     mgs2_area_name, kMgs2SpecialItems,
     difficulty_standard, requirement_mgs2, summary_extra_mgs2, summary_area_mgs2, nullptr},
    {codename::evaluate_mgs3, codename::elite_requirements_mgs3, kMgs3Tabs,
     "FOXHOUND tracker", apply_theme_mgs3, kMgs3Colors, match_color_default,
     mgs3_area_name, kMgs3SpecialItems,
     difficulty_standard, nullptr, summary_extra_mgs3, nullptr, nullptr},
    {codename::evaluate_mgs4, codename::elite_requirements_mgs4, kMgs4Tabs,
     "BIG BOSS tracker", apply_theme_mgs4, kMgs4Colors, match_color_default,
     mgs4_area_name, kMgs4SpecialItems,
     difficulty_mgs4, nullptr, summary_extra_mgs4, nullptr,
     codename::all_matches_mgs4},
    {codename::evaluate_mgspw, codename::elite_requirements_mgspw, kMgspwTabs,
     "FOXHOUND tracker", apply_theme_mgspw, kMgspwColors, match_color_default, nullptr, {},
     difficulty_standard, nullptr, nullptr, nullptr, nullptr},
    {codename::evaluate_babel, codename::elite_requirements_babel, kGenericTabs,
     "BIG BOSS tracker", apply_theme_babel, kBabelColors, match_color_babel, nullptr, {},
     difficulty_babel, nullptr, nullptr, nullptr, nullptr},
};

static_assert(std::size(kOverlays) == 8);

const GameOverlay& overlay_for(Game game)
{
    const size_t index = static_cast<size_t>(game);
    return kOverlays[index < std::size(kOverlays) ? index : 0];
}

IdColors id_colors(Game game)
{
    return overlay_for(game).colors;
}

const char* area_name(Game game, const char* code)
{
    const auto lookup = overlay_for(game).area_name;
    return lookup ? lookup(code) : nullptr;
}

void apply_game_theme()
{
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;
    overlay_for(g_game).theme(style);
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
            const auto eval_fn = overlay_for(g_game).evaluate;
            g_eval.match = eval_fn ? eval_fn(stats) : std::optional<codename::Match>{};
            const auto req_fn = overlay_for(g_game).requirements;
            g_eval.reqs = req_fn ? req_fn(stats) : std::vector<codename::ReqStatus>{};
            const auto all_matches = overlay_for(g_game).all_matches;
            g_eval.matches = all_matches ? all_matches(stats) : std::vector<codename::Match>{};
        }
        next_poll = now + kStatsPollMs;
    }
    const char* panel_title = overlay_for(g_game).title;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiCond initial_layout = g.reset_window ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
    const ImVec2 anchor(viewport->WorkPos.x + ui_size(16),
                        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(anchor, initial_layout, ImVec2(0.0f, 0.5f));
    push_font(g_font_semibold, kSectionScale);
    ImGui::Begin(panel_title, nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
                     | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PopFont();
    static float placed_height = -1.0f;
    const float panel_height = ImGui::GetWindowHeight();
    if (panel_height != placed_height) {
        ImGui::SetWindowPos(ImVec2(anchor.x, anchor.y - panel_height * 0.5f));
        placed_height = panel_height;
    }
    g.reset_window = false;

    if (!have_stats) {
        ImGui::TextDisabled("waiting for game data");
        ImGui::End();
        return;
    }

    static int selected_tab = 0;
    const std::span<const PanelTab> tabs = overlay_for(g_game).tabs;
    const bool tab_bar = tabs.size() > 1;
    if (tab_bar && key_pressed(kTabKey)) {
        selected_tab = (selected_tab + 1) % static_cast<int>(tabs.size());
    }
    const bool scroll_up = key_pressed(VK_UP);
    const bool scroll_down = key_pressed(VK_DOWN);
    const int scroll = scroll_up ? -1 : scroll_down ? 1 : 0;
    if (tab_bar && ImGui::BeginTabBar("tracker_tabs")) {
        for (int i = 0; i < static_cast<int>(tabs.size()); ++i) {
            push_font(selected_tab == i ? g_font_bold : g_font_semibold, 1.0f);
            const bool open = ImGui::BeginTabItem(tabs[i].title, nullptr,
                                                  selected_tab == i
                                                      ? ImGuiTabItemFlags_SetSelected : 0);
            ImGui::PopFont();
            if (open) {
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
    HWND wnd = CreateWindowExA(0, wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0,
                              kDummyWindowSize, kDummyWindowSize,
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
    HWND wnd = CreateWindowExA(0, wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0,
                              kDummyWindowSize, kDummyWindowSize,
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
    swap_desc.Width = kDummyWindowSize;
    swap_desc.Height = kDummyWindowSize;
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
    LOG_INFO("bbtracker %s starting (%s), toggle key vk=0x%02X", BB_VERSION, g_label,
             kToggleKey);

    bool logged_wait_module = false;
    bool logged_wait_renderer = false;
    bool use_d3d12 = false;
    for (;;) {
        if (!GetModuleHandleW(game_module)) {
            if (!logged_wait_module) {
                LOG_INFO("waiting for game module %S", game_module);
                logged_wait_module = true;
            }
            Sleep(kModuleRetryMs);
            continue;
        }
        HMODULE d3d11 = GetModuleHandleW(L"d3d11.dll");
        HMODULE d3d12 = GetModuleHandleW(L"d3d12.dll");
        if (!d3d11 && !d3d12) {
            if (!logged_wait_renderer) {
                LOG_INFO("waiting for d3d11.dll or d3d12.dll");
                logged_wait_renderer = true;
            }
            Sleep(kModuleRetryMs);
            continue;
        }
        use_d3d12 = !d3d11 && d3d12;
        break;
    }

    while (!(use_d3d12 ? install_d3d12_hooks() : install_d3d11_hooks())) {
        LOG_WARN("hook install failed, retrying in 5s");
        Sleep(kHookRetryMs);
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
        if (++timing_samples == kProbeTimingSampleCount) {
            LOG_INFO("probe timing: avg=%lluus max=%lluus",
                     static_cast<unsigned long long>(total_ticks * kMicrosecondsPerSecond
                                                     / frequency.QuadPart / timing_samples),
                     static_cast<unsigned long long>(max_ticks * kMicrosecondsPerSecond
                                                     / frequency.QuadPart));
            total_ticks = 0;
            max_ticks = 0;
            timing_samples = 0;
        }
        // 10 Hz: the live per-mission tallies are read here, and at 4 Hz the
        // sampling lag was visible in game. PW probe costs ~80us per poll.
        Sleep(kStatsPollMs);
    }
}

} // namespace bb
