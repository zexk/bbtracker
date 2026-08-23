#include "probe.h"

#include <windows.h>

#include <cstring>
#include <cstdint>

#include "../../common/log.h"

namespace bb::mgs3 {
namespace {

constexpr uintptr_t kMainPointerOffset = 0x00ACDE98;
constexpr size_t kStatsRegionSize = 0x600;
constexpr wchar_t kModuleName[] = L"METAL GEAR SOLID3.exe";

struct StatOffsets {
    constexpr static size_t kDifficulty = 0x06;
    constexpr static size_t kContinues = 0x34;
    constexpr static size_t kSaves = 0x36;
    constexpr static size_t kAlerts = 0x38;
    constexpr static size_t kKills = 0x3A;
    constexpr static size_t kSpecialItems = 0x3D;
    constexpr static size_t kPlantsCaptured = 0x3F;
    constexpr static size_t kSevereInjuries = 0x40;
    constexpr static size_t kTotalDamage = 0x42;
    constexpr static size_t kMealsEaten = 0x46;
    constexpr static size_t kGameTimeFrames = 0x4C;
    constexpr static size_t kLifeMeds = 0x5A8;
};

const uint8_t* g_stats = nullptr;

bool range_readable(uintptr_t addr, size_t len)
{
    const uintptr_t end = addr + len;
    while (addr < end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) {
            return false;
        }
        if (mbi.State != MEM_COMMIT) {
            return false;
        }
        constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY
            | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if ((mbi.Protect & kReadable) == 0 || (mbi.Protect & PAGE_GUARD) != 0) {
            return false;
        }
        const uintptr_t region_end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        addr = region_end;
    }
    return true;
}

bool resolve()
{
    if (g_stats) {
        return true;
    }
    HMODULE mod = GetModuleHandleW(kModuleName);
    if (!mod) {
        return false;
    }
    const auto base = reinterpret_cast<uintptr_t>(mod);
    const uintptr_t slot = base + kMainPointerOffset;
    if (!range_readable(slot, sizeof(uintptr_t))) {
        return false;
    }
    const uintptr_t ptr = *reinterpret_cast<volatile const uintptr_t*>(slot);
    if (!ptr || !range_readable(ptr, kStatsRegionSize)) {
        return false;
    }

    g_stats = reinterpret_cast<const uint8_t*>(ptr);

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(mod);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    LOG_INFO("mgs3 stats resolved: stats=%p module_timestamp=0x%08X",
             reinterpret_cast<const void*>(g_stats),
             static_cast<unsigned>(nt->FileHeader.TimeDateStamp));
    return true;
}

template <typename T>
T read_at(size_t offset)
{
    T v{};
    std::memcpy(&v, g_stats + offset, sizeof(T));
    return v;
}

} // namespace

bool poll_stats(GameStats& out)
{
    if (!resolve()) {
        return false;
    }
    if (!range_readable(reinterpret_cast<uintptr_t>(g_stats), kStatsRegionSize)) {
        LOG_WARN("mgs3 stats region no longer readable; re-resolving next frame");
        g_stats = nullptr;
        return false;
    }

    out.continues = read_at<uint16_t>(StatOffsets::kContinues);
    out.saves = read_at<uint16_t>(StatOffsets::kSaves);
    out.alerts = read_at<uint16_t>(StatOffsets::kAlerts);
    out.kills = read_at<uint16_t>(StatOffsets::kKills);

    out.special_item_used = read_at<uint8_t>(StatOffsets::kSpecialItems) != 0;
    out.plants_captured = read_at<uint8_t>(StatOffsets::kPlantsCaptured);
    out.severe_injuries = read_at<uint16_t>(StatOffsets::kSevereInjuries);
    out.damage_taken_bars = static_cast<float>(read_at<uint32_t>(StatOffsets::kTotalDamage));
    out.meals_eaten = read_at<uint16_t>(StatOffsets::kMealsEaten);
    out.play_time_seconds =
        static_cast<double>(read_at<uint32_t>(StatOffsets::kGameTimeFrames)) / 60.0;
    out.life_med_used = read_at<uint16_t>(StatOffsets::kLifeMeds);

    switch (read_at<uint8_t>(StatOffsets::kDifficulty)) {
    case 0: out.difficulty = Difficulty::VeryEasy; break;
    case 1: out.difficulty = Difficulty::Easy; break;
    case 2: out.difficulty = Difficulty::Normal; break;
    case 3: out.difficulty = Difficulty::Hard; break;
    default: out.difficulty = Difficulty::Extreme; break;
    }
    out.difficulty_raw = read_at<uint8_t>(StatOffsets::kDifficulty);

    return true;
}

} // namespace bb::mgs3
