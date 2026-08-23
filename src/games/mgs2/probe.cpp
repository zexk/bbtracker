#include "probe.h"

#include <windows.h>

#include <cstring>
#include <cstdint>

#include "../../common/config.h"
#include "../../common/log.h"

namespace bb::mgs2 {
namespace {

constexpr uintptr_t kPlayerPointerOffset = 0x00949340;
constexpr size_t kStatsBlockOffset = 0x12E;
constexpr size_t kRationsOffset = 0x1590;
constexpr size_t kSpecialItemsOffset = 0x1596;
constexpr size_t kPlayerRegionSize = 0x1600;
constexpr wchar_t kModuleName[] = L"METAL GEAR SOLID2.exe";
constexpr uint8_t kGametypeTanker = 16;
constexpr uint8_t kGametypeTP = 32;

struct StatOffsets {
    constexpr static size_t kGametype = 0x07;
    constexpr static size_t kDifficulty = 0x10;
    constexpr static size_t kContinues = 4;
    constexpr static size_t kSaves = 8;
    constexpr static size_t kPlayTimeFrames = 10;
    constexpr static size_t kShots = 18;
    constexpr static size_t kAlerts = 20;
    constexpr static size_t kKills = 22;
    constexpr static size_t kDamage = 24;
};

uintptr_t g_last_player = 0;

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

template <typename T>
T read_at(uintptr_t base, size_t offset)
{
    T v{};
    std::memcpy(&v, reinterpret_cast<const uint8_t*>(base) + offset, sizeof(T));
    return v;
}

} // namespace

bool poll_stats(GameStats& out)
{
    HMODULE mod = GetModuleHandleW(kModuleName);
    if (!mod) {
        return false;
    }
    const auto base = reinterpret_cast<uintptr_t>(mod);
    const uintptr_t slot = base + kPlayerPointerOffset;
    if (!range_readable(slot, sizeof(uintptr_t))) {
        return false;
    }
    const uintptr_t player = *reinterpret_cast<volatile const uintptr_t*>(slot);
    if (!player || !range_readable(player, kPlayerRegionSize)) {
        return false;
    }
    if (player != g_last_player) {
        LOG_INFO("mgs2 player block %s%p", g_last_player ? "moved: " : "",
                 reinterpret_cast<const void*>(player));
        g_last_player = player;
    }

    out.continues = read_at<uint16_t>(player, kStatsBlockOffset + StatOffsets::kContinues);
    out.saves = read_at<uint16_t>(player, kStatsBlockOffset + StatOffsets::kSaves);
    out.alerts = read_at<uint16_t>(player, kStatsBlockOffset + StatOffsets::kAlerts);
    out.kills = read_at<uint16_t>(player, kStatsBlockOffset + StatOffsets::kKills);
    out.shots_fired = read_at<uint16_t>(player, kStatsBlockOffset + StatOffsets::kShots);
    out.damage_taken_units =
        read_at<uint16_t>(player, kStatsBlockOffset + StatOffsets::kDamage);
    out.play_time_seconds =
        static_cast<double>(read_at<uint32_t>(player, kStatsBlockOffset + StatOffsets::kPlayTimeFrames))
        / 60.0;
    out.rations_used = read_at<uint16_t>(player, kRationsOffset);
    out.special_item_used = read_at<uint16_t>(player, kSpecialItemsOffset) != 0;

    switch (read_at<uint8_t>(player, StatOffsets::kDifficulty)) {
    case 10: out.difficulty = Difficulty::VeryEasy; break;
    case 20: out.difficulty = Difficulty::Easy; break;
    case 30: out.difficulty = Difficulty::Normal; break;
    case 40: out.difficulty = Difficulty::Hard; break;
    case 60: out.difficulty = Difficulty::EuroExtreme; break;
    default: out.difficulty = Difficulty::Extreme; break;
    }
    out.difficulty_raw = read_at<uint8_t>(player, StatOffsets::kDifficulty);

    switch (read_at<uint8_t>(player, StatOffsets::kGametype)) {
    case kGametypeTanker: out.mission = 16; break;
    case kGametypeTP: out.mission = 32; break;
    default: out.mission = 0; break;
    }

    if (config().difficulty_override >= 0) {
        out.difficulty = static_cast<Difficulty>(config().difficulty_override);
        out.difficulty_raw = static_cast<uint8_t>(config().difficulty_override);
    }

    return true;
}

} // namespace bb::mgs2
