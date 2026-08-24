#include "probe.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

namespace bb::mg12 {
namespace {

bool readable(uintptr_t address, size_t size)
{
    MEMORY_BASIC_INFORMATION memory{};
    if (!VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory))) return false;
    constexpr DWORD access = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY
        | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return memory.State == MEM_COMMIT && (memory.Protect & access) != 0
        && (memory.Protect & PAGE_GUARD) == 0
        && address + size <= reinterpret_cast<uintptr_t>(memory.BaseAddress) + memory.RegionSize;
}

template <typename T>
T read(uintptr_t address)
{
    T value{};
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
    return value;
}

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

} // namespace

bool poll_mg1(GameStats& out)
{
    const auto module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"mg1.dll"));
    constexpr uintptr_t first = 0x2F6A4;
    constexpr uintptr_t last = 0x2F780;
    if (!module || !readable(module + first, last - first + sizeof(uint32_t))) return false;
    set_common(out, read<uint32_t>(module + 0x2F6A4), read<uint32_t>(module + 0x2F768),
               read<uint32_t>(module + 0x2F76C), read<uint32_t>(module + 0x2F770),
               read<uint32_t>(module + 0x2F774), read<uint32_t>(module + 0x2F778),
               read<uint32_t>(module + 0x2F780), 15.0);
    return true;
}

bool poll_mg2(GameStats& out)
{
    const auto module = reinterpret_cast<uintptr_t>(GetModuleHandleW(L"mg2.dll"));
    if (!module || !readable(module + 0x45790, 0x1C)
        || !readable(module + 0x46DE0, sizeof(uintptr_t))) return false;
    const uintptr_t state = read<uintptr_t>(module + 0x46DE0);
    if (!state || !readable(state + 0x88, sizeof(uint32_t))) return false;
    set_common(out, read<uint32_t>(state + 0x88), read<uint32_t>(module + 0x45790),
               read<uint32_t>(module + 0x45794), read<uint32_t>(module + 0x45798),
               read<uint32_t>(module + 0x4579C), read<uint32_t>(module + 0x457A0),
               read<uint32_t>(module + 0x457A8), 60.0);
    return true;
}

} // namespace bb::mg12
