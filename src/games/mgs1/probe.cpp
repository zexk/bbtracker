#include "probe.h"

#include <windows.h>
#include <psapi.h>

#include <array>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <vector>

#include "../../common/log.h"

namespace bb::mgs1 {
namespace {

constexpr wchar_t kModuleName[] = L"METAL GEAR SOLID.exe";
constexpr size_t kWorkRegionSize = 0x200;
constexpr std::array<size_t, 3> kGameTimeOffsets{0x939D, 0x9495, 0x9B11};
constexpr double kGameTimeFramesPerSecond = 60.0;

uintptr_t g_array_start = 0;
unsigned g_zero_polls = 0;
int g_last_diff = 999;
bool g_radar_seen_on = false;
uint8_t g_last_radar_state = 0xFF;
uint32_t g_last_game_frames = 0;
std::array<uint32_t, kGameTimeOffsets.size()> g_time_samples{};
uint64_t g_time_sample_tick = 0;
int g_game_time_index = -1;
bool g_integral = false;

bool range_readable(uintptr_t addr, size_t len);

enum class PsxVariant { Unknown, WesternOriginal, JapaneseOriginal, Integral };

struct SerialRule {
    std::string_view text;
    PsxVariant variant;
};

// Disc serials surface in memory either as PSX paths ("cdrom:\SLPM_862.47;1")
// or plain product codes ("SLPM-86247"); both spellings are listed.
// Serials verified against Master Collection Vol.1 windata/alldata.bin and
// windata/dlc/dlc_japan.bin; unrecognized serials keep stats hidden.
constexpr std::array kSerialRules{
    // JP original Premium Package (dlc_japan.bin, both discs)
    SerialRule{"SLPM_861.11", PsxVariant::JapaneseOriginal},
    SerialRule{"SLPM-86111", PsxVariant::JapaneseOriginal},
    SerialRule{"SLPM_861.12", PsxVariant::JapaneseOriginal},
    SerialRule{"SLPM-86112", PsxVariant::JapaneseOriginal},

    // Integral (dlc_japan.bin, both discs)
    SerialRule{"SLPM_862.47", PsxVariant::Integral},
    SerialRule{"SLPM-86247", PsxVariant::Integral},
    SerialRule{"SLPM_862.48", PsxVariant::Integral},
    SerialRule{"SLPM-86248", PsxVariant::Integral},
    SerialRule{"SLPM_862.49", PsxVariant::Integral},
    SerialRule{"SLPM-86249", PsxVariant::Integral},

    // Western original: US/EU campaign discs
    SerialRule{"SLUS_005.94", PsxVariant::WesternOriginal},
    SerialRule{"SLUS-00594", PsxVariant::WesternOriginal},
    SerialRule{"SLUS_007.76", PsxVariant::WesternOriginal},
    SerialRule{"SLUS-00776", PsxVariant::WesternOriginal},
    SerialRule{"SLES_013.70", PsxVariant::WesternOriginal},
    SerialRule{"SLES-01370", PsxVariant::WesternOriginal},
    SerialRule{"SLES_113.70", PsxVariant::WesternOriginal},
    SerialRule{"SLES-11370", PsxVariant::WesternOriginal},
    SerialRule{"SLES_015.06", PsxVariant::WesternOriginal},
    SerialRule{"SLES-01506", PsxVariant::WesternOriginal},
    SerialRule{"SLES_115.06", PsxVariant::WesternOriginal},
    SerialRule{"SLES-11506", PsxVariant::WesternOriginal},
    SerialRule{"SLES_015.07", PsxVariant::WesternOriginal},
    SerialRule{"SLES-01507", PsxVariant::WesternOriginal},
    SerialRule{"SLES_115.07", PsxVariant::WesternOriginal},
    SerialRule{"SLES-11507", PsxVariant::WesternOriginal},
    SerialRule{"SLES_015.08", PsxVariant::WesternOriginal},
    SerialRule{"SLES-01508", PsxVariant::WesternOriginal},
    SerialRule{"SLES_115.08", PsxVariant::WesternOriginal},
    SerialRule{"SLES-11508", PsxVariant::WesternOriginal},
    SerialRule{"SLES_017.34", PsxVariant::WesternOriginal},
    SerialRule{"SLES-01734", PsxVariant::WesternOriginal},
    SerialRule{"SLES_117.34", PsxVariant::WesternOriginal},
    SerialRule{"SLES-11734", PsxVariant::WesternOriginal},
};

constexpr PsxVariant classify_serial(std::string_view text)
{
    for (const SerialRule& rule : kSerialRules) {
        if (text.find(rule.text) != std::string_view::npos) {
            return rule.variant;
        }
    }
    return PsxVariant::Unknown;
}

constexpr const char* variant_name(PsxVariant v)
{
    switch (v) {
    case PsxVariant::JapaneseOriginal: return "JP original";
    case PsxVariant::Integral: return "Integral";
    case PsxVariant::WesternOriginal: return "western original";
    default: return "unknown";
    }
}

static_assert(classify_serial("cdrom:\\SLPM_861.11;1") == PsxVariant::JapaneseOriginal);
static_assert(classify_serial("SLPM-86248") == PsxVariant::Integral);
static_assert(classify_serial("SLPM_862.49;1") == PsxVariant::Integral);
static_assert(classify_serial("SLUS_005.94;1") == PsxVariant::WesternOriginal);
static_assert(classify_serial("SLES_013.70;1") != PsxVariant::JapaneseOriginal);
static_assert(classify_serial("garbage") == PsxVariant::Unknown);

struct FieldOffsets {
    constexpr static size_t kStage = 0x03;
    constexpr static size_t kRadarState = 0x0C;
    constexpr static size_t kDifficulty = 0x15;
    constexpr static size_t kCurrentHealth = 0x29;
    constexpr static size_t kMaxHealth = 0x2B;
    constexpr static size_t kDiazepamTimer = 0xA5;
    constexpr static size_t kAlerts = 0xAF;
    constexpr static size_t kKills = 0xB1;
    constexpr static size_t kRationsUsed = 0xBF;
    constexpr static size_t kContinues = 0xC1;
    constexpr static size_t kSaves = 0xC3;
};

static_assert(FieldOffsets::kRadarState == FieldOffsets::kStage + 9);

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

struct SerialHit {
    PsxVariant variant = PsxVariant::Unknown;
    char serial[16]{};
};

SerialHit scan_disc_serial()
{
    static uintptr_t cursor = 0x10000;

    // The tracker's own image embeds every serial string, so exclude self.
    HMODULE self = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&scan_disc_serial), &self)) {
        return {};
    }
    MODULEINFO self_info{};
    if (!GetModuleInformation(GetCurrentProcess(), self, &self_info, sizeof(self_info))) {
        return {};
    }
    const uintptr_t self_begin = reinterpret_cast<uintptr_t>(self_info.lpBaseOfDll);
    const uintptr_t self_end = self_begin + self_info.SizeOfImage;

    LARGE_INTEGER frequency{};
    LARGE_INTEGER started{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&started);
    const int64_t deadline = started.QuadPart + frequency.QuadPart / 1000;

    MEMORY_BASIC_INFORMATION mbi{};
    constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY
        | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    while (VirtualQuery(reinterpret_cast<LPCVOID>(cursor), &mbi, sizeof(mbi))) {
        const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        const uintptr_t end = base + mbi.RegionSize;
        if ((base < self_begin || base >= self_end) && mbi.State == MEM_COMMIT
            && (mbi.Protect & kReadable) != 0
            && (mbi.Protect & PAGE_GUARD) == 0 && mbi.RegionSize <= 0x4000000) {
            const char* next = reinterpret_cast<const char*>(cursor > base ? cursor : base);
            const char* const region_limit = reinterpret_cast<const char*>(end);
            while (next < region_limit) {
                constexpr size_t kScanChunkSize = 0x100000;
                const char* const search_limit = static_cast<size_t>(region_limit - next)
                        < kScanChunkSize
                    ? region_limit
                    : next + kScanChunkSize;
                const auto* found = static_cast<const char*>(
                    std::memchr(next, 'S', static_cast<size_t>(search_limit - next)));
                if (!found) {
                    next = search_limit;
                } else {
                    next = found + 1;
                    if (region_limit - found >= 4
                        && (std::memcmp(found + 1, "LPM", 3) == 0
                            || std::memcmp(found + 1, "LUS", 3) == 0
                            || std::memcmp(found + 1, "LES", 3) == 0)) {
                        const std::string_view candidate{
                            found, static_cast<size_t>(region_limit - found)};
                        for (const SerialRule& rule : kSerialRules) {
                            if (candidate.starts_with(rule.text)) {
                                SerialHit hit{};
                                hit.variant = rule.variant;
                                rule.text.copy(hit.serial, sizeof(hit.serial) - 1);
                                cursor = 0x10000;
                                return hit;
                            }
                        }
                    }
                }
                LARGE_INTEGER current{};
                QueryPerformanceCounter(&current);
                if (current.QuadPart >= deadline) {
                    cursor = reinterpret_cast<uintptr_t>(next);
                    return {};
                }
            }
        }
        if (end <= cursor) {
            break;
        }
        cursor = end;
    }
    cursor = 0x10000;
    return {};
}

PsxVariant g_variant = PsxVariant::Unknown;
char g_serial[sizeof(SerialHit{}.serial)]{};

PsxVariant disc_variant()
{
    static uint64_t next_scan = 0;
    if (g_variant != PsxVariant::Unknown) {
        return g_variant;
    }
    const uint64_t now = GetTickCount64();
    if (now < next_scan) {
        return PsxVariant::Unknown;
    }
    next_scan = now + 250;
    const SerialHit hit = scan_disc_serial();
    if (hit.variant == PsxVariant::Unknown) {
        return PsxVariant::Unknown;
    }
    g_variant = hit.variant;
    std::memcpy(g_serial, hit.serial, sizeof(g_serial));
    LOG_INFO("mgs1 disc serial %s -> %s", g_serial, variant_name(g_variant));
    return g_variant;
}

template <typename T>
T read_at(uintptr_t base, size_t offset)
{
    T v{};
    std::memcpy(&v, reinterpret_cast<const uint8_t*>(base) + offset, sizeof(T));
    return v;
}

constexpr uint32_t clock_score(uint32_t delta, uint32_t expected)
{
    if (delta < expected / 2 || delta > expected * 2 + 10) {
        return UINT32_MAX;
    }
    return delta > expected ? delta - expected : expected - delta;
}

static_assert(clock_score(60, 60) == 0);
static_assert(clock_score(0, 60) == UINT32_MAX);

constexpr bool clock_reset(uint32_t previous, uint32_t current)
{
    return current < previous && current <= 60;
}

static_assert(clock_reset(1234, 0));
static_assert(!clock_reset(0, 0));

uint32_t read_game_frames()
{
    std::array<uint32_t, kGameTimeOffsets.size()> current{};
    for (size_t i = 0; i < kGameTimeOffsets.size(); ++i) {
        if (g_array_start <= kGameTimeOffsets[i]
            || !range_readable(g_array_start - kGameTimeOffsets[i], sizeof(uint32_t))) {
            continue;
        }
        current[i] = read_at<uint32_t>(g_array_start - kGameTimeOffsets[i], 0);
    }
    if (g_game_time_index >= 0) {
        const uint32_t selected = current[static_cast<size_t>(g_game_time_index)];
        if (clock_reset(g_last_game_frames, selected)) {
            LOG_INFO("mgs1 new emulated session; resetting variant detection");
            g_variant = PsxVariant::Unknown;
            g_serial[0] = '\0';
            g_game_time_index = -1;
            g_time_sample_tick = 0;
            return 0;
        }
        return selected;
    }

    const uint64_t now = GetTickCount64();
    if (g_time_sample_tick == 0) {
        g_time_samples = current;
        g_time_sample_tick = now;
        return 0;
    }
    const uint64_t elapsed = now - g_time_sample_tick;
    if (elapsed < 1000) {
        return 0;
    }
    const uint32_t expected = static_cast<uint32_t>(elapsed * kGameTimeFramesPerSecond / 1000.0);
    uint32_t best_score = UINT32_MAX;
    for (size_t i = 0; i < current.size(); ++i) {
        const uint32_t delta = current[i] >= g_time_samples[i]
            ? current[i] - g_time_samples[i]
            : UINT32_MAX;
        const uint32_t score = clock_score(delta, expected);
        if (score < best_score) {
            best_score = score;
            g_game_time_index = static_cast<int>(i);
        }
    }
    g_time_samples = current;
    g_time_sample_tick = now;
    if (g_game_time_index < 0) {
        return 0;
    }
    LOG_INFO("mgs1 game-time offset -0x%04X", static_cast<unsigned>(
                                                    kGameTimeOffsets[g_game_time_index]));
    return current[static_cast<size_t>(g_game_time_index)];
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

int candidate_score(uintptr_t p)
{
    if (!plausible_work_array(p)) {
        return -1;
    }
    int score = 1;
    const int8_t difficulty = read_at<int8_t>(p, FieldOffsets::kDifficulty);
    const uint16_t health = read_at<uint16_t>(p, FieldOffsets::kCurrentHealth);
    const uint16_t max_health = read_at<uint16_t>(p, FieldOffsets::kMaxHealth);
    score += difficulty >= -1 && difficulty <= 3 ? 4 : 0;
    score += max_health > 0 && max_health <= 10000 && health <= max_health ? 3 : 0;
    score += read_at<uint16_t>(p, FieldOffsets::kAlerts) < 10000 ? 1 : 0;
    score += read_at<uint16_t>(p, FieldOffsets::kKills) < 10000 ? 1 : 0;
    return score;
}

constexpr bool valid_stage_name(std::string_view stage)
{
    if (stage == "opening" || stage == "title") {
        return true;
    }
    if (stage.size() < 4 || stage.size() > 7 || (stage[0] != 's' && stage[0] != 'd')
        || stage[1] < '0' || stage[1] > '9' || stage[2] < '0' || stage[2] > '9') {
        return false;
    }
    for (size_t i = 3; i < stage.size(); ++i) {
        if (stage[i] < 'a' || stage[i] > 'z') {
            return false;
        }
    }
    return true;
}

bool looks_like_stage_anchor(const uint8_t* p)
{
    if (p[0] != 0 || p[1] != 0 || p[2] != 0) {
        return false;
    }
    size_t len = 0;
    while (len < 7 && p[FieldOffsets::kStage + len] != 0) {
        ++len;
    }
    return len < 7 && valid_stage_name({
                          reinterpret_cast<const char*>(p) + FieldOffsets::kStage, len});
}

static_assert(FieldOffsets::kStage == 3);
static_assert(valid_stage_name("s00a"));
static_assert(valid_stage_name("s19br"));
static_assert(!valid_stage_name("FindFir"));

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

bool find_candidate(uintptr_t& out)
{
    // ponytail: startup latch can take ~12s because this scans process memory. Replace with a
    // verified PSX RAM anchor when available; keep this scan as the compatibility fallback.
    struct Candidate {
        uintptr_t address;
        std::array<uint32_t, kGameTimeOffsets.size()> clocks;
        uint64_t tick;
    };
    static uintptr_t cursor = 0x10000;
    static std::vector<Candidate> candidates;
    static uint64_t next_scan = 0;

    const uint64_t now_ms = GetTickCount64();
    for (Candidate& candidate : candidates) {
        const uint64_t elapsed = now_ms - candidate.tick;
        const uint32_t expected = static_cast<uint32_t>(
            elapsed * kGameTimeFramesPerSecond / 1000.0);
        for (size_t i = 0; i < candidate.clocks.size(); ++i) {
            const uintptr_t clock = candidate.address - kGameTimeOffsets[i];
            const uint32_t current = range_readable(clock, sizeof(uint32_t))
                ? read_at<uint32_t>(clock, 0)
                : 0;
            const uint32_t delta = current >= candidate.clocks[i]
                ? current - candidate.clocks[i]
                : UINT32_MAX;
            if ((clock_reset(candidate.clocks[i], current)
                 || (elapsed >= 1000 && clock_score(delta, expected) != UINT32_MAX))
                && candidate_score(candidate.address) >= 8) {
                out = candidate.address;
                g_game_time_index = static_cast<int>(i);
                candidates.clear();
                cursor = 0x10000;
                return true;
            }
            if (elapsed >= 1000) {
                candidate.clocks[i] = current;
            }
        }
        if (elapsed >= 1000) {
            candidate.tick = now_ms;
        }
    }

    if (now_ms < next_scan) {
        return false;
    }
    next_scan = now_ms + 1000;

    MEMORY_BASIC_INFORMATION mbi{};
    constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY
        | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    while (VirtualQuery(reinterpret_cast<LPCVOID>(cursor), &mbi, sizeof(mbi))) {
        const uintptr_t vbase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        const uintptr_t region_end = vbase + mbi.RegionSize;

        const bool readable =
            mbi.State == MEM_COMMIT && (mbi.Protect & kReadable) != 0
            && (mbi.Protect & PAGE_GUARD) == 0 && mbi.RegionSize >= 0x200
            && mbi.RegionSize <= 0x4000000;
        if (readable) {
            const uint8_t* begin = reinterpret_cast<const uint8_t*>(vbase);
            const size_t len = mbi.RegionSize;
            size_t i = cursor > vbase ? cursor - vbase : 0;
            for (; i + kWorkRegionSize <= len; ++i) {
                if (looks_like_stage_anchor(begin + i) && !is_logger_table(vbase + i)) {
                    bool known = false;
                    for (const Candidate& candidate : candidates) {
                        known |= candidate.address == vbase + i;
                    }
                    if (!known) {
                        Candidate candidate{vbase + i, {}, GetTickCount64()};
                        for (size_t clock = 0; clock < candidate.clocks.size(); ++clock) {
                            const uintptr_t address = candidate.address - kGameTimeOffsets[clock];
                            candidate.clocks[clock] = range_readable(address, sizeof(uint32_t))
                                ? read_at<uint32_t>(address, 0)
                                : 0;
                        }
                        candidates.push_back(candidate);
                    }
                }
            }
        }

        if (region_end <= cursor) {
            break;
        }
        cursor = region_end;
    }
    out = 0;
    cursor = 0x10000;
    return false;
}

const uint8_t* g_stats_stage()
{
    return reinterpret_cast<const uint8_t*>(g_array_start) + FieldOffsets::kStage;
}

} // namespace

bool poll_stats(GameStats& out)
{
    if (!g_array_start || !range_readable(g_array_start, kWorkRegionSize)) {
        g_array_start = 0;
        uintptr_t candidate = 0;
        if (!find_candidate(candidate)) {
            return false;
        }
        g_array_start = candidate;
        if (!g_array_start) {
            return false;
        }
        const int best_score = candidate_score(g_array_start);
        char stage[8]{};
        std::memcpy(stage,
                    reinterpret_cast<const uint8_t*>(g_array_start) + FieldOffsets::kStage, 7);
        LOG_INFO("mgs1 work array at %p stage=%s score=%d",
                 reinterpret_cast<const void*>(g_array_start), stage, best_score);
        log_hex_dump(reinterpret_cast<const uint8_t*>(g_array_start), 0xD0);
        g_integral = disc_variant() == PsxVariant::Integral;
        g_time_sample_tick = 0;
    }

    out.alerts = read_at<uint16_t>(g_array_start, FieldOffsets::kAlerts);
    out.kills = read_at<uint16_t>(g_array_start, FieldOffsets::kKills);
    out.rations_used = read_at<uint16_t>(g_array_start, FieldOffsets::kRationsUsed);
    out.continues = read_at<uint16_t>(g_array_start, FieldOffsets::kContinues);
    out.saves = read_at<uint16_t>(g_array_start, FieldOffsets::kSaves);
    out.current_health = read_at<uint16_t>(g_array_start, FieldOffsets::kCurrentHealth);
    out.max_health = read_at<uint16_t>(g_array_start, FieldOffsets::kMaxHealth);
    const int16_t diazepam = read_at<int16_t>(g_array_start, FieldOffsets::kDiazepamTimer);
    out.diazepam_frames = diazepam > 0 && diazepam <= 1200 ? diazepam : 0;
    const uint32_t game_frames = read_game_frames();
    if (game_frames > 0) {
        out.play_time_seconds = game_frames / kGameTimeFramesPerSecond;
        if (game_frames + 30 < g_last_game_frames) {
            g_radar_seen_on = false;
        }
        g_last_game_frames = game_frames;
    }

    const uint8_t radar_state = read_at<uint8_t>(g_array_start, FieldOffsets::kRadarState);
    const uint8_t stage_prefix = read_at<uint8_t>(g_array_start, FieldOffsets::kStage);
    const bool gameplay = stage_prefix == 's' || stage_prefix == 'd';
    if (gameplay && radar_state == 0x00) {
        g_radar_seen_on = true;
    }
    out.radar_off = gameplay && radar_state == 0x20 && !g_radar_seen_on;
    if (radar_state != g_last_radar_state) {
        LOG_INFO("mgs1 radar state=0x%02X (%s)", static_cast<unsigned>(radar_state),
                 out.radar_off ? "off" : "on/unknown");
        g_last_radar_state = radar_state;
    }

    const PsxVariant variant = disc_variant();
    g_integral = variant == PsxVariant::Integral;
    out.mgs1_integral = g_integral;
    out.mgs1_japanese_original = variant == PsxVariant::JapaneseOriginal;
    if (g_game_time_index >= 0 && variant != PsxVariant::Unknown) {
        const int expected = out.mgs1_japanese_original ? 1 : 0;
        if (g_game_time_index != expected) {
            static bool warned_mismatch = false;
            if (!warned_mismatch) {
                warned_mismatch = true;
                LOG_WARN("mgs1 %s serial but game-time offset -0x%04X (expected -0x%04X)",
                         variant_name(variant),
                         static_cast<unsigned>(kGameTimeOffsets[g_game_time_index]),
                         static_cast<unsigned>(kGameTimeOffsets[expected]));
            }
        }
    }
    const int8_t diff = out.mgs1_japanese_original
        ? 0
        : read_at<int8_t>(g_array_start, FieldOffsets::kDifficulty);
    if (diff != g_last_diff) {
        LOG_INFO("mgs1 difficulty=%d%s", static_cast<int>(diff),
                 out.mgs1_japanese_original ? " (JP fixed)" : "");
        g_last_diff = diff;
    }
    out.difficulty_game_byte = static_cast<uint8_t>(diff);
    switch (diff) {
    case -1: out.difficulty = Difficulty::VeryEasy; break;
    case 0: out.difficulty = Difficulty::Easy; break;
    case 1: out.difficulty = Difficulty::Normal; break;
    case 2: out.difficulty = Difficulty::Hard; break;
    default: out.difficulty = Difficulty::Extreme; break;
    }
    out.difficulty_raw = static_cast<uint8_t>(diff);

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
        if (stage[i] == '\0') {
            break;
        }
        if (stage[i] < ' ' || static_cast<uint8_t>(stage[i]) > 0x7E) {
            stage[i] = '?';
        }
    }
    static char last_stage[8] = {};
    if (std::strcmp(stage, last_stage) != 0) {
        LOG_INFO("mgs1 stage: %s", stage);
        std::memcpy(last_stage, stage, sizeof(last_stage));
    }
    std::memcpy(out.area_code, stage, sizeof(out.area_code));

    return g_game_time_index >= 0 && gameplay && variant != PsxVariant::Unknown;
}

} // namespace bb::mgs1
