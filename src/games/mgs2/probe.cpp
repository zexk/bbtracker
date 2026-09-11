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
#include "../../common/stats.h"
#include "../../overlay/overlay.h"
#include "../asi_main.h"
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
constexpr uint32_t kDogTags2002 = 0x00008000;
constexpr uintptr_t kGclVariableBufRva = 0x017D51E0;
constexpr size_t kLinkVarBufSize = 5528; // MAX_LINKVARBUF
constexpr size_t kVarBufSize = 7168;     // MAX_VAR_BUF
constexpr size_t kStoryOffset = 0x68;    // $w:p_story

// NewGclVariableMove relocates linkvarbuf inside gcl_variable_buf. In one
// layout linkvarbuf is near the start and var_buf follows it; in the other
// var_buf is first and linkvarbuf sits one MAX_VAR_BUF past it.
uintptr_t var_buf_address(uintptr_t module_base, uintptr_t linkvarbuf)
{
    const uintptr_t gcl_buf = module_base + kGclVariableBufRva;
    const uintptr_t delta = linkvarbuf - gcl_buf;
    return delta < kLinkVarBufSize ? linkvarbuf + 2 * kLinkVarBufSize
                                   : linkvarbuf - kVarBufSize;
}

uint16_t read_p_story(uintptr_t var_buf)
{
    uint8_t bytes[2] = {};
    if (!mem::copy(var_buf + kStoryOffset, bytes, sizeof(bytes))) {
        return 0xFFFF;
    }
    return static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
}

uint32_t read_dog_tag_flags(uintptr_t var_buf)
{
    uint32_t flags = 0;
    size_t index = 0;
    for (const DogTagFlag& flag : kDogTagFlags) {
        uint8_t byte = 0;
        if (mem::copy(var_buf + flag.offset, byte) && ((byte >> flag.bit) & 1u) != 0) {
            flags |= uint32_t{1} << index;
        }
        ++index;
    }
    return flags;
}

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

// Chapter intro: opening telop ("muse" = museum) and demo/cutscene stages.
constexpr bool intro_area(std::string_view area)
{
    return area == "muse" || (!area.empty() && area[0] == 'd');
}

constexpr RunState run_state(uint16_t configuration, std::string_view area)
{
    if (menu_area(area)) return RunState::Inactive;
    if (intro_area(area) || (configuration & kStoryFlags) != 0
        || (area.size() == 4 && area[0] == 'w')) {
        return RunState::Active;
    }
    return RunState::Unknown;
}

static_assert(run_state(kStoryTankerActive, "w00a") == RunState::Active);
static_assert(run_state(0, "w00a") == RunState::Active);
static_assert(run_state(0x4000, "d001") == RunState::Active);
static_assert(run_state(0, "muse") == RunState::Active);
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
    constexpr static size_t kConfiguration2 = 0x08;
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
    const uint32_t configuration2 = read_at<uint32_t>(data, StatOffsets::kConfiguration2);
    out.radar_type = configuration & kRadarSettingMask;
    out.mgs2_dog_tags_2002 = (configuration2 & kDogTags2002) != 0;
    const uintptr_t var_buf = var_buf_address(base, player);
    out.mgs2_p_story = read_p_story(var_buf);
    out.mgs2_dog_tag_flags = read_dog_tag_flags(var_buf);
    static uintptr_t last_var_buf = 0;
    if (var_buf != last_var_buf) {
        LOG_INFO("mgs2 var_buf %p (linkvar %p)", reinterpret_cast<const void*>(var_buf),
                 reinterpret_cast<const void*>(player));
        last_var_buf = var_buf;
    }
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
    static uint32_t last_configuration2 = 0xFFFFFFFF;
    if (configuration2 != last_configuration2) {
        LOG_INFO("mgs2 configuration2 0x%08x, dog tags %s", configuration2,
                 out.mgs2_dog_tags_2002 ? "2002" : "2001");
        last_configuration2 = configuration2;
    }
    static uint16_t last_p_story = 0xFFFF;
    static uint32_t last_dog_tag_flags = 0xFFFFFFFF;
    if (out.mgs2_p_story != last_p_story || out.mgs2_dog_tag_flags != last_dog_tag_flags) {
        LOG_INFO("mgs2 p_story %u, dog tag flags 0x%02x",
                 static_cast<unsigned>(out.mgs2_p_story),
                 static_cast<unsigned>(out.mgs2_dog_tag_flags));
        last_p_story = out.mgs2_p_story;
        last_dog_tag_flags = out.mgs2_dog_tag_flags;
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

void bb::asi_main()
{
    start_overlay(BB_GAME_LABEL, &mgs2::poll_stats, L"METAL GEAR SOLID2.exe", Game::MGS2);
}
