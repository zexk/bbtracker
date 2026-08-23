#include "probe.h"

#include <windows.h>
#include <psapi.h>

#include <cstring>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "../../common/config.h"
#include "../../common/log.h"

namespace bb::mgs1 {
namespace {

constexpr wchar_t kModuleName[] = L"METAL GEAR SOLID.exe";
constexpr size_t kWorkRegionSize = 0x200;

constexpr uint8_t kSig[] = {0x00, 0x00, 0x00, 0x6F, 0x70, 0x65, 0x6E, 0x69, 0x6E,
                            0x67, 0x00};

uintptr_t g_array_start = 0;
unsigned g_zero_polls = 0;
uint16_t g_last_diff = 0xFFFF;
std::vector<uintptr_t> g_hunt_hits;
uint16_t g_hunt_last = 0xFFFF;
uint64_t g_hunt_last_scan = 0;

bool range_readable(uintptr_t addr, size_t len);

void hunt_filter_or_scan(uint16_t want)
{
    if (!g_hunt_hits.empty()) {
        std::vector<uintptr_t> keep;
        for (uintptr_t p : g_hunt_hits) {
            if (range_readable(p, 2)) {
                uint16_t v{};
                std::memcpy(&v, reinterpret_cast<const void*>(p), 2);
                if (v == want) {
                    keep.push_back(p);
                }
            }
        }
        if (!keep.empty() || static_cast<uint16_t>(g_hunt_last) == want) {
            g_hunt_hits = std::move(keep);
        }
    }

    if (g_hunt_hits.empty()) {
        MEMORY_BASIC_INFORMATION mbi{};
        uintptr_t addr = 0x10000;
        constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY
            | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        while (VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) {
            const uintptr_t vbase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            const uintptr_t region_end = vbase + mbi.RegionSize;
            if (mbi.State == MEM_COMMIT && (mbi.Protect & kReadable) != 0
                && (mbi.Protect & PAGE_GUARD) == 0) {
                for (uintptr_t p = vbase; p + 2 <= region_end; p += 2) {
                    uint16_t v{};
                    std::memcpy(&v, reinterpret_cast<const void*>(p), 2);
                    if (v == want) {
                        g_hunt_hits.push_back(p);
                        if (g_hunt_hits.size() >= 8192) {
                            goto done;
                        }
                    }
                }
            }
            if (region_end <= addr) {
                break;
            }
            addr = region_end;
        }
    done:
        char sample[256]{};
        size_t pos = 0;
        for (size_t i = 0; i < g_hunt_hits.size() && i < 6; ++i) {
            pos += snprintf(sample + pos, sizeof(sample) - pos, " %p",
                            reinterpret_cast<const void*>(g_hunt_hits[i]));
        }
        LOG_INFO("hunt u16==%u: %zu hit(s)%s", static_cast<unsigned>(want),
                 g_hunt_hits.size(), sample);
    }
}

void run_hunt()
{
    const int expect = config().hunt_value;
    if (expect < 0 || expect > 0xFFFF) {
        return;
    }
    const auto want = static_cast<uint16_t>(expect);
    if (want != g_hunt_last) {
        g_hunt_last = want;
        g_hunt_hits.clear();
    }
    const uint64_t now = GetTickCount64();
    if (now - g_hunt_last_scan < 2000) {
        return;
    }
    g_hunt_last_scan = now;
    hunt_filter_or_scan(want);
}

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

bool is_logger_table(uintptr_t p)
{
    static constexpr char kLoggerTail[] = " session";
    char tail[9]{};
    std::memcpy(tail, reinterpret_cast<const uint8_t*>(p) + FieldOffsets::kStage + 7, 8);
    return std::memcmp(tail, kLoggerTail, 8) == 0;
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
                if (ok && !is_logger_table(vbase + i) && plausible_work_array(vbase + i)) {
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

const uint8_t* g_stats_stage()
{
    return reinterpret_cast<const uint8_t*>(g_array_start) + FieldOffsets::kStage;
}

} // namespace

bool poll_stats(GameStats& out)
{
    static uint64_t last_cfg_reload = 0;
    const uint64_t now_ms = GetTickCount64();
    if (now_ms - last_cfg_reload > 5000) {
        last_cfg_reload = now_ms;
        Config tmp{};
        std::filesystem::path dir = [] {
            wchar_t path[MAX_PATH]{};
            HMODULE mod = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                   | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&poll_stats), &mod);
            GetModuleFileNameW(mod, path, MAX_PATH);
            return std::filesystem::path(path).parent_path();
        }();
        load_config((dir / L"bbtracker.ini").string().c_str(), tmp);
    }

    run_hunt();

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
        log_hex_dump(reinterpret_cast<const uint8_t*>(g_array_start), 0xD0);
    }

    out.alerts = read_at<uint16_t>(g_array_start, FieldOffsets::kAlerts);
    out.kills = read_at<uint16_t>(g_array_start, FieldOffsets::kKills);
    out.rations_used = read_at<uint16_t>(g_array_start, FieldOffsets::kRationsUsed);
    out.continues = read_at<uint16_t>(g_array_start, FieldOffsets::kContinues);
    out.saves = read_at<uint16_t>(g_array_start, FieldOffsets::kSaves);

    const uint16_t diff = read_at<uint16_t>(g_array_start, FieldOffsets::kDifficulty);
    if (diff != g_last_diff) {
        LOG_INFO("workarray+0x15 u16=%u (0x%04X)", static_cast<unsigned>(diff),
                 static_cast<unsigned>(diff));
        g_last_diff = diff;
    }    out.difficulty_game_byte = static_cast<uint8_t>(diff);
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

    char stage[8]{};
    std::memcpy(stage, g_stats_stage(), 7);
    stage[7] = '\0';
    for (int i = 0; i < 7; ++i) {
        if (stage[i] < ' ' || static_cast<uint8_t>(stage[i]) > 0x7E) {
            stage[i] = '?';
        }
    }
    static char last_stage[8] = {};
    if (std::strcmp(stage, last_stage) != 0) {
        LOG_INFO("mgs1 stage: %s", stage);
        std::memcpy(last_stage, stage, sizeof(last_stage));
        std::memcpy(out.area_code, stage, sizeof(out.area_code));
    }

    run_hunt();

    return true;
}

} // namespace bb::mgs1
