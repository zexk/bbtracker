#include "probe.h"

#include <windows.h>

#include <cstring>
#include <cstdint>
#include <string_view>

#include "../../common/area.h"
#include "../../common/difficulty.h"
#include "../../common/log.h"
#include "../../common/mem.h"
#include "../../common/run_latch.h"

namespace bb::mgs2 {

using bb::mem::range_readable;
using bb::mem::read_at;

namespace {

constexpr uintptr_t kPlayerPointerOffset = 0x00949340;
constexpr size_t kStatsBlockOffset = 0x12E;
constexpr size_t kRationsOffset = 0x1590;
constexpr size_t kShipwormOffset = 0x158C;
constexpr size_t kClearingEscapesOffset = 0x1592;
constexpr size_t kSpecialItemsOffset = 0x1596;
constexpr size_t kTimesSeenOffset = 0x1594;
constexpr size_t kPlayerRegionSize = 0x1600;
constexpr size_t kTitleMenuStatusOffset = 0x158A;
constexpr wchar_t kModuleName[] = L"METAL GEAR SOLID2.exe";
constexpr uint8_t kCampaignTanker = 16;
constexpr uint8_t kCampaignPlant = 32;
constexpr uint8_t kCampaignTankerAndPlant = kCampaignTanker | kCampaignPlant;
constexpr uint16_t kStorySelectionMask = 0x0003;
constexpr uint16_t kStoryTanker = 0x0001;
constexpr uint16_t kStoryPlant = 0x0002;
constexpr uint16_t kStoryTankerAndPlant = 0x0003;
constexpr uint16_t kRadarSettingMask = 0x0024;
constexpr uint16_t kSpecialItemUsed = 0x0020;
constexpr uint16_t kRadarUsed = 0x2000;

constexpr uint16_t used_special_items(uint16_t clear_code_flags)
{
    return (clear_code_flags >> 8) & 0x1F;
}

static_assert(used_special_items(0x1F00) == 0x1F);
static_assert(used_special_items(0x001F) == 0);

constexpr bool ranked_campaign(uint8_t campaign)
{
    return campaign == kCampaignTanker || campaign == kCampaignPlant
        || campaign == kCampaignTankerAndPlant;
}

static_assert(ranked_campaign(kCampaignTanker));
static_assert(ranked_campaign(kCampaignPlant));
static_assert(ranked_campaign(kCampaignTankerAndPlant));
static_assert(!ranked_campaign(0));
static_assert(!ranked_campaign(0x40));

constexpr RunState run_state(uint8_t campaign, std::string_view area)
{
    if (ranked_campaign(campaign)) return RunState::Active;
    if (campaign == 0 && area.size() == 4 && area[0] == 'w') return RunState::Unknown;
    return RunState::Inactive;
}

static_assert(run_state(kCampaignTanker, "w00a") == RunState::Active);
static_assert(run_state(0, "w00a") == RunState::Unknown);
static_assert(run_state(0, "") == RunState::Inactive);
static_assert(run_state(0x40, "w00a") == RunState::Inactive);

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
    // Low byte of campaign word. +0x07 is its transition/state high byte.
    constexpr static size_t kCampaign = 0x06;
    constexpr static size_t kDifficulty = 0x10;
    constexpr static size_t kContinues = 4;
    constexpr static size_t kSaves = 8;
    constexpr static size_t kPlayTimeFrames = 10;
    constexpr static size_t kShots = 18;
    constexpr static size_t kAlerts = 20;
    constexpr static size_t kKills = 22;
    constexpr static size_t kDamage = 24;
    constexpr static size_t kPullUps = 14;
    constexpr static size_t kMechsDestroyed = 42;
    constexpr static size_t kCurrentHealth = 250;
    constexpr static size_t kMaxHealth = 252;
};

HMODULE g_module = nullptr;
RunLatch g_run;

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
    const uintptr_t slot = base + kPlayerPointerOffset;
    if (!range_readable(slot, sizeof(uintptr_t))) {
        g_module = nullptr;
        return g_run.hold(out);
    }
    const uintptr_t player = *reinterpret_cast<volatile const uintptr_t*>(slot);
    if (!player || !range_readable(player, kPlayerRegionSize)) {
        return g_run.hold(out);
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
    out.pull_ups = read_at<uint16_t>(player, kStatsBlockOffset + StatOffsets::kPullUps);
    out.mechs_destroyed =
        read_at<uint16_t>(player, kStatsBlockOffset + StatOffsets::kMechsDestroyed);
    out.current_health = read_at<uint16_t>(player, StatOffsets::kCurrentHealth);
    out.max_health = read_at<uint16_t>(player, StatOffsets::kMaxHealth);
    out.damage_taken_bars = out.max_health > 0
        ? (out.damage_taken_units + 50) / out.max_health
        : -1.0;
    out.play_time_seconds =
        static_cast<double>(read_at<uint32_t>(player, kStatsBlockOffset + StatOffsets::kPlayTimeFrames))
        / 60.0;
    out.rations_used = read_at<uint16_t>(player, kRationsOffset);
    out.sea_louse = read_at<uint16_t>(player, kShipwormOffset) != 0;
    out.clearing_escapes = read_at<uint16_t>(player, kClearingEscapesOffset);
    out.times_seen = read_at<uint16_t>(player, kTimesSeenOffset);
    const uint16_t clear_code_flags = read_at<uint16_t>(player, kSpecialItemsOffset);
    out.special_items_mask = used_special_items(clear_code_flags);
    out.special_item_used = (clear_code_flags & kSpecialItemUsed) != 0;
    out.radar_type = read_at<uint16_t>(player, StatOffsets::kCampaign) & kRadarSettingMask;
    // Codename judge checks whether radar was ever used, not current setting.
    out.radar_off = (clear_code_flags & kRadarUsed) == 0;

    const uint8_t raw_difficulty = read_at<uint8_t>(player, StatOffsets::kDifficulty);
    out.difficulty = master_collection_difficulty(raw_difficulty);
    out.difficulty_raw = raw_difficulty;
    out.difficulty_game_byte = raw_difficulty;

    const uint8_t campaign = read_at<uint8_t>(player, StatOffsets::kCampaign);
    static uint8_t last_campaign = 0xFF;
    if (campaign != last_campaign) {
        LOG_INFO("mgs2 campaign %u (ranked %d), difficulty %u", campaign,
                 ranked_campaign(campaign) ? 1 : 0, out.difficulty_raw);
        last_campaign = campaign;
    }
    out.mission = codename_mission(read_at<uint16_t>(player, kTitleMenuStatusOffset));

    char area[8]{};
    std::memcpy(area, reinterpret_cast<const uint8_t*>(player) + StatOffsets::kAreaCode, 4);
    for (int i = 0; i < 4; ++i) {
        const char c = area[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
            area[i] = '\0';
            break;
        }
    }
    set_area(out, area);

    return g_run.update(out, run_state(campaign, out.area_code));
}

} // namespace bb::mgs2
