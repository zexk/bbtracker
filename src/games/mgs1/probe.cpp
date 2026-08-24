#include "probe.h"

#include <windows.h>
#include <psapi.h>

#include <array>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "../../common/log.h"

namespace bb::mgs1 {
namespace {

constexpr wchar_t kModuleName[] = L"METAL GEAR SOLID.exe";
constexpr size_t kWorkRegionSize = 0x200;
constexpr std::array<size_t, 2> kGameTimeOffsets{0x939D, 0x9495};
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

bool range_readable(uintptr_t addr, size_t len);

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
        return current[static_cast<size_t>(g_game_time_index)];
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
    static uintptr_t cursor = 0x10000;
    static uintptr_t best = 0;
    static int best_score = -1;
    LARGE_INTEGER frequency{};
    LARGE_INTEGER started{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&started);
    const int64_t deadline = started.QuadPart + frequency.QuadPart / 1000;
    unsigned checks = 0;
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
                if ((checks++ & 0xFFF) == 0) {
                    LARGE_INTEGER now{};
                    QueryPerformanceCounter(&now);
                    if (now.QuadPart >= deadline) {
                        cursor = vbase + i;
                        return false;
                    }
                }
                if (looks_like_stage_anchor(begin + i) && !is_logger_table(vbase + i)) {
                    const int score = candidate_score(vbase + i);
                    if (score >= 8 && score > best_score) {
                        best = vbase + i;
                        best_score = score;
                    }
                }
            }
        }

        if (region_end <= cursor) {
            break;
        }
        cursor = region_end;
    }
    out = best;
    cursor = 0x10000;
    best = 0;
    best_score = -1;
    return true;
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
        g_time_sample_tick = 0;
        g_game_time_index = -1;
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

    out.mgs1_japanese_original = g_game_time_index == 1;
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

    return true;
}

} // namespace bb::mgs1
