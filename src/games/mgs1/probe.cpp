#include "probe.h"

#include <windows.h>
#include <psapi.h>

#include <cstring>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "../../common/config.h"
#include "../../common/log.h"

namespace bb::mgs1 {
namespace {

constexpr wchar_t kModuleName[] = L"METAL GEAR SOLID.exe";
constexpr size_t kWorkRegionSize = 0x200;

uint8_t kSig[] = {0x00, 0x00, 0x00, 0x6F, 0x70, 0x65, 0x6E, 0x69, 0x6E, 0x67};

uintptr_t g_array_start = 0;
unsigned g_zero_polls = 0;
uint8_t g_last_diff = 0xFF;

struct FieldOffsets {
    constexpr static size_t kStage = 0x03;
    constexpr static size_t kDifficulty = 0x15;
    constexpr static size_t kAlerts = 0xAF;
    constexpr static size_t kKills = 0xB1;
    constexpr static size_t kRationsUsed = 0xBF;
    constexpr static size_t kContinues = 0xC1;
    constexpr static size_t kSaves = 0xC3;
};

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

bool plausible_work_array(uintptr_t p)
{
    if (!range_readable(p, kWorkRegionSize)) {
        return false;
    }
    char stage[12]{};
    std::memcpy(stage, reinterpret_cast<const uint8_t*>(p) + FieldOffsets::kStage, 7);
    for (int i = 0; i < 7; ++i) {
        const char c = stage[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
            || c == '_' || c == '\0';
        if (!ok) {
            return false;
        }
        if (c == '\0') {
            return i >= 3;
        }
    }
    return true;
}

std::vector<uintptr_t> find_candidates()
{
    std::vector<uintptr_t> out;
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t addr = 0x10000;
    constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY
        | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    while (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) {
        const uintptr_t vbase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        const uintptr_t region_end = vbase + mbi.RegionSize;

        const bool readable =
            mbi.State == MEM_COMMIT && (mbi.Protect & kReadable) != 0
            && (mbi.Protect & PAGE_GUARD) == 0 && mbi.RegionSize >= 0x200
            && mbi.RegionSize <= 0x4000000;
        if (readable) {
            const uint8_t* begin = reinterpret_cast<const uint8_t*>(vbase);
            const size_t len = mbi.RegionSize;
            for (size_t i = 0; i + sizeof(kSig) <= len; ++i) {
                if (begin[i] != 0 || begin[i + 1] != 0 || begin[i + 2] != 0
                    || begin[i + 3] != kSig[3]) {
                    continue;
                }
                bool ok = true;
                for (size_t j = 4; j < sizeof(kSig); ++j) {
                    if (begin[i + j] != kSig[j]) {
                        ok = false;
                        break;
                    }
                }
                if (ok && plausible_work_array(vbase + i)) {
                    out.push_back(vbase + i);
                }
            }
        }

        if (region_end <= addr) {
            break;
        }
        addr = region_end;
    }
    return out;
}

} // namespace

bool poll_stats(GameStats& out)
{
    if (!g_array_start || !range_readable(g_array_start, kWorkRegionSize)) {
        g_array_start = 0;
        static uint64_t last_scan = 0;
        const uint64_t now = GetTickCount64();
        if (now - last_scan < 4000) {
            return false;
        }
        last_scan = now;
        auto candidates = find_candidates();
        LOG_INFO("mgs1 work-array scan: %zu candidate(s)", candidates.size());
        for (uintptr_t p : candidates) {
            if (plausible_work_array(p)) {
                g_array_start = p;
                break;
            }
        }
        if (!g_array_start) {
            return false;
        }
        char stage[8]{};
        std::memcpy(stage,
                    reinterpret_cast<const uint8_t*>(g_array_start) + FieldOffsets::kStage, 7);
        LOG_INFO("mgs1 work array at %p stage=%s", reinterpret_cast<const void*>(g_array_start),
                 stage);
    }

    out.alerts = read_at<uint16_t>(g_array_start, FieldOffsets::kAlerts);
    out.kills = read_at<uint16_t>(g_array_start, FieldOffsets::kKills);
    out.rations_used = read_at<uint16_t>(g_array_start, FieldOffsets::kRationsUsed);
    out.continues = read_at<uint16_t>(g_array_start, FieldOffsets::kContinues);
    out.saves = read_at<uint16_t>(g_array_start, FieldOffsets::kSaves);

    const uint16_t diff = read_at<uint16_t>(g_array_start, FieldOffsets::kDifficulty);
    if (diff != g_last_diff) {
        LOG_INFO("difficulty byte: %u", static_cast<unsigned>(diff));
        g_last_diff = static_cast<uint8_t>(diff);
    }
    out.difficulty_game_byte = static_cast<uint8_t>(diff);
    switch (diff) {
    case 0: out.difficulty = Difficulty::VeryEasy; break;
    case 1: out.difficulty = Difficulty::Easy; break;
    case 2: out.difficulty = Difficulty::Normal; break;
    case 3: out.difficulty = Difficulty::Hard; break;
    case 4: out.difficulty = Difficulty::Extreme; break;
    default:
        out.difficulty = config().difficulty_override >= 0
            ? out.difficulty
            : Difficulty::Extreme;
        break;
    }
    if (config().difficulty_override >= 0) {
        out.difficulty = static_cast<Difficulty>(config().difficulty_override);
        out.difficulty_raw = static_cast<uint8_t>(config().difficulty_override);
    } else {
        out.difficulty_raw = static_cast<uint8_t>(diff);
    }

    if (out.alerts == 0 && out.kills == 0 && out.saves == 0 && out.continues == 0) {
        if (++g_zero_polls == 600) {
            LOG_WARN("mgs1 stats all-zero ~10s while polling");
        }
    } else {
        g_zero_polls = 0;
    }

    return true;
}

} // namespace bb::mgs1
