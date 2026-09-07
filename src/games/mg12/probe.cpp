#include "probe.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "../../common/mem.h"

namespace bb::mg12 {

using bb::mem::range_readable;
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
    return state == 8;
}

static_assert(mg1_run_active(8));
static_assert(!mg1_run_active(0));

HMODULE g_mg1 = nullptr;
HMODULE g_mg2 = nullptr;

} // namespace

bool poll_mg1(GameStats& out)
{
    // The game dlls stay loaded for the session: resolve once each, and only
    // re-resolve after a module-relative read fails and drops the latch.
    if (!g_mg1) {
        g_mg1 = GetModuleHandleW(L"mg1.dll");
    }
    const auto module = reinterpret_cast<uintptr_t>(g_mg1);
    constexpr uintptr_t first = 0x2F6A4;
    constexpr uintptr_t last = 0x2F780;
    if (!module || !range_readable(module + 0x2E260, sizeof(uint32_t))
        || !range_readable(module + first, last - first + sizeof(uint32_t))) {
        g_mg1 = nullptr;
        return false;
    }
    set_common(out, read<uint32_t>(module + 0x2F6A4), read<uint32_t>(module + 0x2F768),
               read<uint32_t>(module + 0x2F76C), read<uint32_t>(module + 0x2F770),
               read<uint32_t>(module + 0x2F774), read<uint32_t>(module + 0x2F778),
               read<uint32_t>(module + 0x2F780), 15.0);
    return mg1_run_active(read<uint32_t>(module + 0x2E260));
}

bool poll_mg2(GameStats& out)
{
    if (!g_mg2) {
        g_mg2 = GetModuleHandleW(L"mg2.dll");
    }
    const auto module = reinterpret_cast<uintptr_t>(g_mg2);
    if (!module || !range_readable(module + 0x39170, sizeof(uint32_t))
        || !range_readable(module + 0x45790, 0x1C)
        || !range_readable(module + 0x46DE0, sizeof(uintptr_t))) {
        g_mg2 = nullptr;
        return false;
    }
    const uintptr_t state = read<uintptr_t>(module + 0x46DE0);
    if (!state || !range_readable(state + 0x88, sizeof(uint32_t))) return false;
    set_common(out, read<uint32_t>(state + 0x88), read<uint32_t>(module + 0x45790),
               read<uint32_t>(module + 0x45794), read<uint32_t>(module + 0x45798),
               read<uint32_t>(module + 0x4579C), read<uint32_t>(module + 0x457A0),
               read<uint32_t>(module + 0x457A8), kMg2TicksPerSecond);
    return mg2_run_active(read<uint32_t>(module + 0x39170));
}

} // namespace bb::mg12
