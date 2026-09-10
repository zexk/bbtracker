#include "probe.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <cstring>

#include "../../common/log.h"
#include "../../common/mem.h"
#include "../../common/run_latch.h"

namespace bb::mg12 {

using bb::mem::read;

namespace {

constexpr double kMg2TicksPerSecond = 15.0;
static_assert((0x627 * 60) / kMg2TicksPerSecond == 105 * 60);

void set_common(GameStats& out, uint32_t difficulty, uint32_t timer, uint32_t rations,
                uint32_t kills, uint32_t alerts, uint32_t special, uint32_t continues,
                double ticks_per_second)
{
    out = {};
    out.difficulty_raw = static_cast<uint8_t>(difficulty);
    out.difficulty_game_byte = static_cast<uint8_t>(difficulty);
    out.difficulty = difficulty == 0 ? Difficulty::Easy : Difficulty::Extreme;
    out.play_time_seconds = timer / ticks_per_second;
    out.rations_used = static_cast<int>(rations);
    out.kills = static_cast<int>(kills);
    out.alerts = static_cast<int>(alerts);
    out.special_item_used = special != 0;
    out.continues = static_cast<int>(continues);
}

constexpr bool mg2_run_active(uint32_t substate)
{
    return substate != 41;
}

static_assert(mg2_run_active(0));
static_assert(mg2_run_active(35));
static_assert(!mg2_run_active(41));

constexpr bool mg1_run_active(uint32_t state)
{
    return state != 0;
}

static_assert(mg1_run_active(8));
static_assert(mg1_run_active(10));
static_assert(!mg1_run_active(0));

HMODULE g_mg1 = nullptr;
HMODULE g_mg2 = nullptr;
RunLatch g_mg1_run;
RunLatch g_mg2_run;
bool g_mg1_logged = false;
bool g_mg2_logged = false;

void log_module_once(HMODULE module, const char* game, bool& logged)
{
    uint32_t timestamp = 0;
    if (!logged && mem::module_timestamp(module, timestamp)) {
        LOG_INFO("%s module timestamp 0x%08X", game, static_cast<unsigned>(timestamp));
        logged = true;
    }
}

} // namespace

bool poll_mg1(GameStats& out)
{
    // The game dlls stay loaded for the session: resolve once each, and only
    // re-resolve after a module-relative read fails and drops the latch.
    if (!g_mg1) {
        g_mg1 = GetModuleHandleW(L"mg1.dll");
    }
    const auto module = reinterpret_cast<uintptr_t>(g_mg1);
    if (g_mg1) log_module_once(g_mg1, "mg1", g_mg1_logged);
    constexpr uintptr_t first = 0x2F6A4;
    constexpr uintptr_t last = 0x2F780;
    uint32_t state = 0;
    std::array<uint8_t, last - first + sizeof(uint32_t)> stats{};
    if (!module || !mem::copy(module + 0x2E260, state)
        || !mem::copy(module + first, stats.data(), stats.size())) {
        g_mg1 = nullptr;
        return g_mg1_run.hold(out);
    }
    const uint32_t difficulty = read<uint32_t>(stats.data(), 0x00);
    if (difficulty > 1) return g_mg1_run.hold(out);
    set_common(out, difficulty, read<uint32_t>(stats.data(), 0xC4),
               read<uint32_t>(stats.data(), 0xC8), read<uint32_t>(stats.data(), 0xCC),
               read<uint32_t>(stats.data(), 0xD0), read<uint32_t>(stats.data(), 0xD4),
               read<uint32_t>(stats.data(), 0xDC), 15.0);
    return g_mg1_run.update(out, mg1_run_active(state) ? RunState::Active
                                                       : RunState::Inactive);
}

bool poll_mg2(GameStats& out)
{
    if (!g_mg2) {
        g_mg2 = GetModuleHandleW(L"mg2.dll");
    }
    const auto module = reinterpret_cast<uintptr_t>(g_mg2);
    if (g_mg2) log_module_once(g_mg2, "mg2", g_mg2_logged);
    uint32_t substate = 0;
    uintptr_t state = 0;
    std::array<uint8_t, 0x1C> stats{};
    if (!module || !mem::copy(module + 0x39170, substate)
        || !mem::copy(module + 0x45790, stats.data(), stats.size())
        || !mem::copy(module + 0x46DE0, state)) {
        g_mg2 = nullptr;
        return g_mg2_run.hold(out);
    }
    uint32_t difficulty = 0;
    if (!state || !mem::copy(state + 0x88, difficulty)) return g_mg2_run.hold(out);
    if (difficulty > 1) return g_mg2_run.hold(out);
    set_common(out, difficulty, read<uint32_t>(stats.data(), 0x00),
               read<uint32_t>(stats.data(), 0x04), read<uint32_t>(stats.data(), 0x08),
               read<uint32_t>(stats.data(), 0x0C), read<uint32_t>(stats.data(), 0x10),
               read<uint32_t>(stats.data(), 0x18), kMg2TicksPerSecond);
    return g_mg2_run.update(out, mg2_run_active(substate) ? RunState::Active
                                                          : RunState::Inactive);
}

} // namespace bb::mg12
