#include "probe.h"

#include <windows.h>
#include <bit>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <iterator>

#include "../../common/log.h"

namespace bb::mgs3 {
namespace {

constexpr uintptr_t kFallbackSlotOffset = 0x00ACDE98;
constexpr uintptr_t kFoodSlotOffset = 0x01D2F110;
constexpr size_t kStatsRegionSize = 0x600;
constexpr wchar_t kModuleName[] = L"METAL GEAR SOLID3.exe";

constexpr uint8_t kStatsSig[] = {0x48, 0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00,
                                 0xF7, 0x41, 0x08, 0x00, 0x40, 0x00, 0x00,
                                 0x75, 0x09, 0x8B, 0x05};
constexpr bool kSigWildcard[std::size(kStatsSig)] = {false, false, false, true,  true,
                                                     true,  false, false, false, false,
                                                     false, false, false, false, false,
                                                     false, false, false};

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
    constexpr static size_t kAreaCode = 0x24;
    constexpr static size_t kLifeMeds = 0x5A8;
    constexpr static size_t kKerotans = 0x6532;
    constexpr static size_t kCaptureMask = 0x18A0;
    constexpr static size_t kInjuries = 0x688;
    constexpr static size_t kInjurySize = 0x0E;
    constexpr static size_t kInjuryType = 0x08;
    constexpr static size_t kInjuryHealth = 0x0C;
    constexpr static size_t kInjuryCount = 50;
};

const uint8_t* g_stats = nullptr;
uintptr_t g_last_block = 0;
uintptr_t g_slot_addr = 0;
unsigned g_zero_polls = 0;
uint16_t g_last_dmg_raw = 0;
uint16_t g_last_damage_bars = 0;
uint8_t g_last_diff06 = 0xFF;
uint8_t g_last_diff04 = 0xFF;
uint16_t g_last_vm_flags = 0xFFFF;
uint16_t g_last_se_flags = 0xFFFF;
uint64_t g_last_kerotan_mask = 0;
uintptr_t g_kerotan_title_block = 0;
bool g_kerotan_load_pending = false;

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

bool sig_match(const uint8_t* p, size_t avail)
{
    if (avail < std::size(kStatsSig)) {
        return false;
    }
    for (size_t i = 0; i < std::size(kStatsSig); ++i) {
        if (!kSigWildcard[i] && p[i] != kStatsSig[i]) {
            return false;
        }
    }
    return true;
}

constexpr bool gameplay_area(const char* area)
{
    return area[0] == 's' || area[0] == 'v';
}

static_assert(gameplay_area("s001a"));
static_assert(gameplay_area("v000a"));
static_assert(!gameplay_area("title"));

bool find_slot_via_sig(HMODULE mod, uintptr_t& out_slot)
{
    const auto base = reinterpret_cast<uintptr_t>(mod);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(mod);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const uintptr_t scan_start = base + nt->OptionalHeader.BaseOfCode;
    const size_t scan_size = nt->OptionalHeader.SizeOfCode;

    for (uintptr_t addr = scan_start; addr < scan_start + scan_size; addr += 0x1000) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))
            || mbi.State != MEM_COMMIT) {
            continue;
        }
        const uintptr_t region_end =
            reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        const uintptr_t stop = region_end < scan_start + scan_size ? region_end : scan_start + scan_size;
        uintptr_t p = addr;
        while (p + std::size(kStatsSig) <= stop) {
            const auto* found = static_cast<const uint8_t*>(std::memchr(
                reinterpret_cast<const void*>(p), kStatsSig[0],
                stop - p - std::size(kStatsSig) + 1));
            if (!found) {
                break;
            }
            p = reinterpret_cast<uintptr_t>(found);
            if (!sig_match(found, stop - p)) {
                ++p;
                continue;
            }
            const int32_t disp = *reinterpret_cast<volatile const int32_t*>(p + 3);
            out_slot = disp + p + 7;
            LOG_INFO("stats slot found via signature at module+%llX",
                     static_cast<unsigned long long>(out_slot - base));
            return true;
        }
        if (region_end > addr + 0x1000) {
            addr = region_end - 0x1000;
        }
    }
    return false;
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
bool resolve(uintptr_t& out_block, uintptr_t& out_story_base)
{
    HMODULE mod = GetModuleHandleW(kModuleName);
    if (!mod) {
        return false;
    }
    const auto base = reinterpret_cast<uintptr_t>(mod);

    if (!g_slot_addr) {
        uintptr_t sig_slot = 0;
        if (find_slot_via_sig(mod, sig_slot)) {
            g_slot_addr = sig_slot;
        } else {
            LOG_WARN("stats signature not found; falling back to static offset");
            g_slot_addr = base + kFallbackSlotOffset;
        }
    }

    const uintptr_t slot = g_slot_addr;
    if (!range_readable(slot, sizeof(uintptr_t))) {
        g_slot_addr = 0;
        return false;
    }
    const uintptr_t ptr = *reinterpret_cast<volatile const uintptr_t*>(slot);
    if (!ptr || !range_readable(ptr, kStatsRegionSize)) {
        return false;
    }
    const uintptr_t story_ptr =
        *reinterpret_cast<volatile const uintptr_t*>(slot + 0x10);

    if (ptr != g_last_block) {
        LOG_INFO("stats block %s%p", g_last_block ? "moved: " : "",
                 reinterpret_cast<const void*>(ptr));
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(mod);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        LOG_INFO("module_timestamp=0x%08X",
                 static_cast<unsigned>(nt->FileHeader.TimeDateStamp));
        log_hex_dump(reinterpret_cast<const uint8_t*>(ptr), 0x50);
        g_last_block = ptr;
        g_zero_polls = 0;
    }

    out_block = ptr;
    out_story_base = story_ptr;
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
    uintptr_t story_base = 0;
    if (!resolve(block, story_base)) {
        return false;
    }
    g_stats = reinterpret_cast<const uint8_t*>(block);

    out.continues = read_at<uint16_t>(StatOffsets::kContinues);
    out.saves = read_at<uint16_t>(StatOffsets::kSaves);
    out.alerts = read_at<uint16_t>(StatOffsets::kAlerts);
    out.kills = read_at<uint16_t>(StatOffsets::kKills);

    out.special_items_mask = read_at<uint8_t>(StatOffsets::kSpecialItems);
    out.special_item_used = out.special_items_mask != 0;
    out.plants_captured = read_at<uint8_t>(StatOffsets::kPlantsCaptured);
    out.severe_injuries = read_at<uint16_t>(StatOffsets::kSevereInjuries);

    const uint16_t dmg_raw = read_at<uint16_t>(StatOffsets::kTotalDamage);
    if (dmg_raw != g_last_dmg_raw) {
        LOG_INFO("dmg@0x42 u16=%u", static_cast<unsigned>(dmg_raw));
        g_last_dmg_raw = dmg_raw;
    }
    out.damage_taken_units = static_cast<int>(dmg_raw);

    const uint16_t damage_bars = read_at<uint16_t>(StatOffsets::kTotalDamage + 2);
    out.damage_taken_bars = damage_bars;
    if (damage_bars != g_last_damage_bars) {
        LOG_INFO("damage bars@0x44 u16=%u", static_cast<unsigned>(damage_bars));
        g_last_damage_bars = damage_bars;
    }

    out.meals_eaten = read_at<uint16_t>(StatOffsets::kMealsEaten);
    out.play_time_seconds =
        static_cast<double>(read_at<uint32_t>(StatOffsets::kGameTimeFrames)) / 60.0;
    out.life_med_used = read_at<uint16_t>(StatOffsets::kLifeMeds);

    uint64_t kerotan_mask = 0;
    if (range_readable(reinterpret_cast<uintptr_t>(g_stats + StatOffsets::kKerotans), 8)) {
        for (size_t i = 0; i < 8; ++i) {
            const uint8_t bits = read_at<uint8_t>(StatOffsets::kKerotans + i);
            kerotan_mask |= static_cast<uint64_t>(bits) << (i * 8);
        }
    }
    const bool at_title = std::memcmp(g_stats + StatOffsets::kAreaCode, "title", 6) == 0;
    if (at_title) {
        g_last_kerotan_mask = 0;
        g_kerotan_title_block = block;
        g_kerotan_load_pending = true;
        kerotan_mask = 0;
    } else if (g_kerotan_load_pending && block == g_kerotan_title_block) {
        kerotan_mask = 0;
    } else if (g_kerotan_load_pending) {
        g_last_kerotan_mask = 0;
        g_kerotan_load_pending = false;
    }
    // Wrapped layout briefly exposes this fill pattern during area transitions.
    if ((kerotan_mask >> 16) == 0xFFFFFFFFFFFFULL) {
        kerotan_mask = g_last_kerotan_mask;
    } else {
        g_last_kerotan_mask |= kerotan_mask;
        kerotan_mask = g_last_kerotan_mask;
    }
    out.kerotan_mask = std::rotr(kerotan_mask, 1);
    out.kerotans = std::popcount(kerotan_mask);

    out.capture_mask = 0;
    if (range_readable(reinterpret_cast<uintptr_t>(g_stats + StatOffsets::kCaptureMask), 6)) {
        for (size_t i = 0; i < 6; ++i) {
            out.capture_mask |= static_cast<uint64_t>(
                                    read_at<uint8_t>(StatOffsets::kCaptureMask + i))
                << (i * 8);
        }
    }

    out.leech_attached = false;
    constexpr size_t kInjuriesSize = StatOffsets::kInjurySize * StatOffsets::kInjuryCount;
    if (range_readable(reinterpret_cast<uintptr_t>(g_stats + StatOffsets::kInjuries),
                       kInjuriesSize)) {
        for (size_t i = 0; i < StatOffsets::kInjuryCount; ++i) {
            const size_t injury = StatOffsets::kInjuries + i * StatOffsets::kInjurySize;
            if (read_at<uint8_t>(injury + StatOffsets::kInjuryType) == 7
                && read_at<uint16_t>(injury + StatOffsets::kInjuryHealth) > 0) {
                out.leech_attached = true;
                break;
            }
        }
    }

    out.tsuchinoko_alive = false;
    if (HMODULE mod = GetModuleHandleW(kModuleName)) {
        const uintptr_t food_slot = reinterpret_cast<uintptr_t>(mod) + kFoodSlotOffset;
        if (range_readable(food_slot, sizeof(uintptr_t))) {
            const uintptr_t food =
                *reinterpret_cast<volatile const uintptr_t*>(food_slot);
            constexpr size_t kCageTypes[] = {0x0, 0x8, 0x10};
            constexpr size_t kCageOccupied[] = {0xFEC, 0x103C, 0x108C};
            if (food && range_readable(food, kCageOccupied[2] + sizeof(uint16_t))) {
                for (size_t i = 0; i < std::size(kCageTypes); ++i) {
                    const auto type = *reinterpret_cast<volatile const uint16_t*>(
                        food + kCageTypes[i]);
                    const auto occupied = *reinterpret_cast<volatile const uint16_t*>(
                        food + kCageOccupied[i]);
                    if (type == 130 && occupied != 0) {
                        out.tsuchinoko_alive = true;
                        break;
                    }
                }
            }
        }
    }

    const uint8_t diff06 = read_at<uint8_t>(StatOffsets::kDifficulty);
    const uint8_t diff04 = read_at<uint8_t>(StatOffsets::kDifficulty - 2);
    if (diff06 != g_last_diff06 || diff04 != g_last_diff04) {
        LOG_INFO("difficulty candidates: @0x04=%u @0x06=%u",
                 static_cast<unsigned>(diff04), static_cast<unsigned>(diff06));
        g_last_diff06 = diff06;
        g_last_diff04 = diff04;
    }
    out.difficulty_game_byte = diff06;
    switch (diff06) {
    case 10: out.difficulty = Difficulty::VeryEasy; break;
    case 20: out.difficulty = Difficulty::Easy; break;
    case 30: out.difficulty = Difficulty::Normal; break;
    case 40: out.difficulty = Difficulty::Hard; break;
    case 60: out.difficulty = Difficulty::EuroExtreme; break;
    default: out.difficulty = Difficulty::Extreme; break;
    }
    if (story_base && range_readable(story_base, 0x40)) {
        const uint16_t story_vm =
            *reinterpret_cast<volatile const uint16_t*>(story_base + 0x2);
        const uint16_t story_se =
            *reinterpret_cast<volatile const uint16_t*>(story_base + 0x4);
        if (story_vm != g_last_vm_flags || story_se != g_last_se_flags) {
            LOG_INFO("story flags vm=0x%04X se=0x%04X",
                     static_cast<unsigned>(story_vm), static_cast<unsigned>(story_se));
            g_last_vm_flags = story_vm;
            g_last_se_flags = story_se;
        }
    }

    char area[8]{};
    std::memcpy(area, g_stats + StatOffsets::kAreaCode, 7);
    for (int i = 0; i < 7; ++i) {
        if ((area[i] < '0' || area[i] > 'z') && area[i] != '_') {
            area[i] = '\0';
            break;
        }
    }
    if (std::strcmp(area, out.area_code) != 0) {
        LOG_INFO("area: %s", area);
        std::memcpy(out.area_code, area, sizeof(out.area_code));
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

    return gameplay_area(out.area_code);
}

} // namespace bb::mgs3
