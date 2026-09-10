#include "probe.h"

#include <windows.h>

#include <array>
#include <cstring>
#include <cstdint>
#include <string_view>

#include "../../common/area.h"
#include "../../common/difficulty.h"
#include "../../common/log.h"
#include "../../common/mem.h"
#include "../../common/run_latch.h"
#include "dog_tags.h"

namespace bb::mgs2 {

using bb::mem::read_at;

namespace {

constexpr uintptr_t kPlayerPointerOffset = 0x00949340;
constexpr size_t kStatsBlockOffset = 0x12E;
constexpr size_t kDogTagFlagsOffset = 0x3C;
constexpr size_t kStagePlayTimeOffset = 0xE4;
constexpr size_t kRationsOffset = 0x1590;
constexpr size_t kShipwormOffset = 0x158C;
constexpr size_t kClearingEscapesOffset = 0x1592;
constexpr size_t kSpecialItemsOffset = 0x1596;
constexpr size_t kTimesSeenOffset = 0x1594;
constexpr size_t kPlayerRegionSize = 0x1600;
constexpr size_t kTitleMenuStatusOffset = 0x158A;
constexpr size_t kSnakePullUpsOffset = 0x12E;
constexpr size_t kRaidenPullUpsOffset = 0x130;
constexpr wchar_t kModuleName[] = L"METAL GEAR SOLID2.exe";
constexpr uint16_t kStorySelectionMask = 0x0003;
constexpr uint16_t kStoryTanker = 0x0001;
constexpr uint16_t kStoryPlant = 0x0002;
constexpr uint16_t kStoryTankerAndPlant = 0x0003;
constexpr uint16_t kStoryTankerActive = 0x1000;
constexpr uint16_t kStoryTankerCleared = 0x2000;
constexpr uint16_t kStoryFlags = kStoryTankerActive | kStoryTankerCleared;
constexpr uint16_t kRadarSettingMask = 0x0024;
constexpr uint16_t kSpecialItemUsed = 0x0020;
constexpr uint16_t kRadarUsed = 0x2000;

constexpr uint16_t used_special_items(uint16_t clear_code_flags)
{
    return (clear_code_flags >> 8) & 0x1F;
}

static_assert(used_special_items(0x1F00) == 0x1F);
static_assert(used_special_items(0x001F) == 0);

constexpr bool menu_area(std::string_view area)
{
    return area == "init" || area == "n_ti" || area == "msel"
        || area == "mkse" || area == "sele" || area == "trme";
}

constexpr RunState run_state(uint16_t configuration, std::string_view area)
{
    if (menu_area(area)) return RunState::Inactive;
    if ((configuration & kStoryFlags) != 0 || (area.size() == 4 && area[0] == 'w')) {
        return RunState::Active;
    }
    return RunState::Unknown;
}

static_assert(run_state(kStoryTankerActive, "w00a") == RunState::Active);
static_assert(run_state(0, "w00a") == RunState::Active);
static_assert(run_state(0x4000, "d001") == RunState::Unknown);
static_assert(run_state(kStoryTankerActive, "n_ti") == RunState::Inactive);
static_assert(run_state(0, "") == RunState::Unknown);

constexpr int codename_mission(uint16_t title_menu_status)
{
    switch (title_menu_status & kStorySelectionMask) {
    case kStoryTanker: return 16;
    case kStoryTankerAndPlant: return 32;
    case kStoryPlant:
    default: return 0;
    }
}

static_assert(codename_mission(kStoryTanker) == 16);
static_assert(codename_mission(kStoryPlant) == 0);
static_assert(codename_mission(kStoryTankerAndPlant) == 32);

uintptr_t g_last_player = 0;

struct StatOffsets {
    constexpr static size_t kAreaCode = 0x2C;
    constexpr static size_t kConfiguration = 0x06;
    constexpr static size_t kDifficulty = 0x10;
    constexpr static size_t kContinues = 4;
    constexpr static size_t kSaves = 8;
    constexpr static size_t kPlayTimeFrames = 10;
    constexpr static size_t kShots = 18;
    constexpr static size_t kAlerts = 20;
    constexpr static size_t kKills = 22;
    constexpr static size_t kDamage = 24;
    constexpr static size_t kMechsDestroyed = 42;
    constexpr static size_t kCurrentHealth = 250;
    constexpr static size_t kMaxHealth = 252;
};

HMODULE g_module = nullptr;
RunLatch g_run;
bool g_logged_module = false;

// The player block moves, but its module does not: resolve once, and only
// re-resolve after a module-relative read fails and drops the latch. A
// null player pointer is normal during loads and keeps the latch.
HMODULE game_module()
{
    if (!g_module) {
        g_module = GetModuleHandleW(kModuleName);
    }
    return g_module;
}

} // namespace

bool poll_stats(GameStats& out)
{
    HMODULE mod = game_module();
    if (!mod) {
        return g_run.hold(out);
    }
    const auto base = reinterpret_cast<uintptr_t>(mod);
    if (!g_logged_module) {
        uint32_t timestamp = 0;
        if (mem::module_timestamp(mod, timestamp)) {
            LOG_INFO("mgs2 module timestamp 0x%08X", static_cast<unsigned>(timestamp));
            g_logged_module = true;
        }
    }
    const uintptr_t slot = base + kPlayerPointerOffset;
    uintptr_t player = 0;
    if (!mem::copy(slot, player)) {
        g_module = nullptr;
        return g_run.hold(out);
    }
    if (!player) {
        return g_run.hold(out);
    }
    std::array<uint8_t, kPlayerRegionSize> snapshot{};
    if (!mem::copy(player, snapshot.data(), snapshot.size())) return g_run.hold(out);
    if (player != g_last_player) {
        LOG_INFO("mgs2 player block %s%p", g_last_player ? "moved: " : "",
                 reinterpret_cast<const void*>(player));
        g_last_player = player;
    }

    const uintptr_t data = reinterpret_cast<uintptr_t>(snapshot.data());
    out = {};
    for (size_t i = 0; i < kDogTagWordCount; ++i) {
        out.dog_tag_mask[i] = read_at<uint32_t>(data, kDogTagFlagsOffset + i * sizeof(uint32_t));
    }
    out.dog_tags = dog_tag_count(out.dog_tag_mask);
    out.continues = read_at<uint16_t>(data, kStatsBlockOffset + StatOffsets::kContinues);
    out.saves = read_at<uint16_t>(data, kStatsBlockOffset + StatOffsets::kSaves);
    out.alerts = read_at<uint16_t>(data, kStatsBlockOffset + StatOffsets::kAlerts);
    out.kills = read_at<uint16_t>(data, kStatsBlockOffset + StatOffsets::kKills);
    out.shots_fired = read_at<uint16_t>(data, kStatsBlockOffset + StatOffsets::kShots);
    out.damage_taken_units =
        read_at<uint16_t>(data, kStatsBlockOffset + StatOffsets::kDamage);
    out.snake_pull_ups = read_at<uint16_t>(data, kSnakePullUpsOffset);
    out.raiden_pull_ups = read_at<uint16_t>(data, kRaidenPullUpsOffset);
    out.mechs_destroyed =
        read_at<uint16_t>(data, kStatsBlockOffset + StatOffsets::kMechsDestroyed);
    out.current_health = read_at<uint16_t>(data, StatOffsets::kCurrentHealth);
    out.max_health = read_at<uint16_t>(data, StatOffsets::kMaxHealth);
    out.damage_taken_bars = out.max_health > 0
        ? (out.damage_taken_units + 50) / out.max_health
        : -1.0;
    out.play_time_seconds =
        static_cast<double>(read_at<uint32_t>(data, kStatsBlockOffset + StatOffsets::kPlayTimeFrames))
        / 60.0;
    out.stage_time_seconds = read_at<uint32_t>(data, kStagePlayTimeOffset) / 60.0;
    out.rations_used = read_at<uint16_t>(data, kRationsOffset);
    out.sea_louse = read_at<uint16_t>(data, kShipwormOffset) != 0;
    out.clearing_escapes = read_at<uint16_t>(data, kClearingEscapesOffset);
    out.times_seen = read_at<uint16_t>(data, kTimesSeenOffset);
    const uint16_t clear_code_flags = read_at<uint16_t>(data, kSpecialItemsOffset);
    out.special_items_mask = used_special_items(clear_code_flags);
    out.special_item_used = (clear_code_flags & kSpecialItemUsed) != 0;
    const uint16_t configuration = read_at<uint16_t>(data, StatOffsets::kConfiguration);
    out.radar_type = configuration & kRadarSettingMask;
    // Codename judge checks whether radar was ever used, not current setting.
    out.radar_off = (clear_code_flags & kRadarUsed) == 0;

    const uint8_t raw_difficulty = read_at<uint8_t>(data, StatOffsets::kDifficulty);
    if (!known_master_collection_difficulty(raw_difficulty)) return g_run.hold(out);
    out.difficulty = master_collection_difficulty(raw_difficulty);
    out.difficulty_raw = raw_difficulty;
    out.difficulty_game_byte = raw_difficulty;

    static uint16_t last_configuration = 0xFFFF;
    if (configuration != last_configuration) {
        LOG_INFO("mgs2 configuration 0x%04x, difficulty %u", configuration,
                 out.difficulty_raw);
        last_configuration = configuration;
    }
    out.mission = codename_mission(read_at<uint16_t>(data, kTitleMenuStatusOffset));

    char area[8]{};
    std::memcpy(area, snapshot.data() + StatOffsets::kAreaCode, 4);
    for (int i = 0; i < 4; ++i) {
        const char c = area[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
            area[i] = '\0';
            break;
        }
    }
    set_area(out, area);

    return g_run.update(out, run_state(configuration, out.area_code));
}

} // namespace bb::mgs2
