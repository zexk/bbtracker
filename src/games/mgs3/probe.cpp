#include "probe.h"

#include <windows.h>

#include <cstring>
#include <cstdint>
#include <cstdio>

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
uintptr_t g_last_block = 0;
unsigned g_zero_polls = 0;

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

void log_hex_dump(const uint8_t* data, size_t len)
{
    for (size_t row = 0; row < len; row += 16) {
        char line[128];
        size_t pos = 0;
        pos += snprintf(line + pos, sizeof(line) - pos, "  %04zx:", row);
        for (size_t i = 0; i < 16 && row + i < len; ++i) {
            pos += snprintf(line + pos, sizeof(line) - pos, " %02X", data[row + i]);
        }
        LOG_INFO("%s", line);
    }
}

bool resolve(uintptr_t& out_block)
{
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

    if (ptr != g_last_block) {
        LOG_INFO("stats block %s%p", g_last_block ? "moved: " : "", reinterpret_cast<const void*>(ptr));
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(mod);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        LOG_INFO("module_timestamp=0x%08X",
                 static_cast<unsigned>(nt->FileHeader.TimeDateStamp));
        log_hex_dump(reinterpret_cast<const uint8_t*>(ptr), 0x50);
        g_last_block = ptr;
        g_zero_polls = 0;
    }

    out_block = ptr;
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
    uintptr_t block = 0;
    if (!resolve(block)) {
        return false;
    }
    g_stats = reinterpret_cast<const uint8_t*>(block);

    out.continues = read_at<uint16_t>(StatOffsets::kContinues);
    out.saves = read_at<uint16_t>(StatOffsets::kSaves);
    out.alerts = read_at<uint16_t>(StatOffsets::kAlerts);
    out.kills = read_at<uint16_t>(StatOffsets::kKills);

    out.special_item_used = read_at<uint8_t>(StatOffsets::kSpecialItems) != 0;
    out.plants_captured = read_at<uint8_t>(StatOffsets::kPlantsCaptured);
    out.severe_injuries = read_at<uint16_t>(StatOffsets::kSevereInjuries);

    const uint32_t dmg_raw = read_at<uint32_t>(StatOffsets::kTotalDamage);
    float dmg_f;
    std::memcpy(&dmg_f, &dmg_raw, sizeof(dmg_f));
    if (dmg_raw <= 5000) {
        out.damage_taken_bars = static_cast<float>(dmg_raw);
    } else if (dmg_f >= 0.0f && dmg_f < 100000.0f) {
        out.damage_taken_bars = dmg_f;
    } else {
        out.damage_taken_bars = static_cast<float>(dmg_raw);
    }

    out.meals_eaten = read_at<uint16_t>(StatOffsets::kMealsEaten);
    out.play_time_seconds =
        static_cast<double>(read_at<uint32_t>(StatOffsets::kGameTimeFrames)) / 60.0;
    out.life_med_used = read_at<uint16_t>(StatOffsets::kLifeMeds);

    const uint8_t diff = read_at<uint8_t>(StatOffsets::kDifficulty);
    out.difficulty_raw = diff;
    switch (diff) {
    case 0: out.difficulty = Difficulty::VeryEasy; break;
    case 1: out.difficulty = Difficulty::Easy; break;
    case 2: out.difficulty = Difficulty::Normal; break;
    case 3: out.difficulty = Difficulty::Hard; break;
    default: out.difficulty = Difficulty::Extreme; break;
    }

    if (out.kills == 0 && out.alerts == 0 && out.saves == 0 && out.continues == 0
        && out.play_time_seconds == 0.0) {
        if (++g_zero_polls == 600) {
            LOG_WARN("stats all-zero for ~10s while polling; pointer may be stale "
                     "for this game build");
            log_hex_dump(g_stats, 0x50);
        }
    } else {
        g_zero_polls = 0;
    }

    return true;
}

} // namespace bb::mgs3
