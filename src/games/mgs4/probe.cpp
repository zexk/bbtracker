#include <windows.h>

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "../../common/area.h"
#include "../../common/difficulty.h"
#include "../../common/log.h"
#include "../../common/mem.h"
#include "../../common/run_latch.h"
#include "../../common/stats.h"
#include "../../overlay/overlay.h"
#include "../asi_main.h"
#include "flashbacks.h"

namespace bb::mgs4 {

using bb::mem::read;

namespace {

constexpr uintptr_t kLinkvarbufPointer = 0x1C28B28;
constexpr size_t kLinkvarbufSize = 0x8344;
constexpr uint32_t kLastValidProgress = 291;
constexpr size_t kStageCodeLength = 7;
constexpr double kFramesPerSecond = 60.0;
constexpr size_t kBitsPerByte = 8;
constexpr size_t kFlashbackCount = std::size(kFlashbacks);
constexpr size_t kLastFlashbackByte = (kFlashbackCount - 1) / kBitsPerByte;
constexpr uint8_t kLastFlashbackByteMask =
    static_cast<uint8_t>((1u << ((kFlashbackCount - 1) % kBitsPerByte + 1)) - 1);
constexpr size_t kFlashbackFlagsOffset = 0x5A49;
constexpr size_t kFirstWeaponId = 1;
constexpr size_t kLastWeaponId = 73;
constexpr size_t kUnusedWeaponId = 11;
constexpr size_t kWeaponStatesOffset = 0x1D4;
// The results screen reads this bit before it offers LITTLE GRAY
// (mgs4.exe 0x1414bb840, `test [linkvarbuf+0x20], 0x4000`).
constexpr size_t kTitleFlagsOffset = 0x20;
constexpr uint32_t kLittleGrayClaimedBit = 0x4000;

static_assert(kFlashbackCount <= sizeof(GameStats{}.flashback_bits) * kBitsPerByte);

// MGS4 shifts the collection codes: Naked Normal is 30, Solid Normal 35.
constexpr DifficultyCode kDifficultyCodes[] = {
    {20, Difficulty::VeryEasy},
    {30, Difficulty::Easy},
    {35, Difficulty::Normal},
    {40, Difficulty::Hard},
    {50, Difficulty::Extreme},
};

constexpr bool known_difficulty(uint16_t value)
{
    for (const DifficultyCode& code : kDifficultyCodes) {
        if (code.code == value) return true;
    }
    return false;
}

Difficulty difficulty(uint16_t value)
{
    return difficulty_from_table(value, kDifficultyCodes, Difficulty::Extreme);
}

constexpr bool ranked_stage(std::string_view stage)
{
    return stage.size() == kStageCodeLength && stage[0] == 's'
            && ((stage[1] == '0' && stage[2] >= '0' && stage[2] <= '5')
                || (stage[1] == '1' && stage[2] == '0')
                || (stage[1] == '2' && stage[2] == '0')
                || (stage[1] == '3' && stage[2] == '0'))
            && stage[3] == 'a'
            && stage[4] >= '0' && stage[4] <= '9'
            && stage[5] >= '0' && stage[5] <= '9'
            && stage[6] == 'l';
}

static_assert(ranked_stage("s01a10l"));
static_assert(ranked_stage("s10a10l"));
static_assert(ranked_stage("s30a10l"));
static_assert(!ranked_stage("r_sna01"));
static_assert(!ranked_stage("title"));
static_assert(!ranked_stage("s99a00l"));

constexpr RunState run_state(std::string_view stage)
{
    if (ranked_stage(stage)) return RunState::Active;
    if (stage == "title") return RunState::Inactive;
    return RunState::Unknown;
}

static_assert(run_state("s01a10l") == RunState::Active);
static_assert(run_state("r_sna01") == RunState::Unknown);
static_assert(run_state("title") == RunState::Inactive);

HMODULE g_module = nullptr;
RunLatch g_run;
bool g_logged_module = false;

} // namespace

bool poll_stats(GameStats& out)
{
    // mgs4.exe never unloads mid-process: resolve once, and only re-resolve
    // after a module-relative read fails and drops the latch.
    if (!g_module) {
        g_module = GetModuleHandleW(nullptr);
    }
    const auto module = reinterpret_cast<uintptr_t>(g_module);
    if (g_module && !g_logged_module) {
        uint32_t timestamp = 0;
        if (mem::module_timestamp(g_module, timestamp)) {
            LOG_INFO("MGS4 module timestamp 0x%08X", static_cast<unsigned>(timestamp));
            g_logged_module = true;
        }
    }
    uintptr_t address = 0;
    if (!module || !mem::copy(module + kLinkvarbufPointer, address)) {
        g_module = nullptr;
        return g_run.hold(out);
    }
    if (!address) return g_run.hold(out);
    std::array<uint8_t, kLinkvarbufSize> snapshot{};
    if (!mem::copy(address, snapshot.data(), snapshot.size())) return g_run.hold(out);
    const auto* data = snapshot.data();

    const uint16_t raw_difficulty = read<uint16_t>(data, 0x006);
    const uint32_t progress = read<uint32_t>(data, 0x054);
    if (!known_difficulty(raw_difficulty) || progress > kLastValidProgress) {
        return g_run.hold(out);
    }

    static uintptr_t last_address = 0;
    if (address != last_address) {
        LOG_INFO("MGS4 linkvarbuf %p (module+0x%llX)", reinterpret_cast<const void*>(address),
                 static_cast<unsigned long long>(kLinkvarbufPointer));
        last_address = address;
    }

    out = {};
    char stage[kStageCodeLength + 1]{};
    std::memcpy(stage, data + 0x034, kStageCodeLength);
    const size_t stage_length = std::char_traits<char>::length(stage);
    set_area(out, stage);
    out.difficulty_raw = static_cast<uint8_t>(raw_difficulty);
    out.difficulty_game_byte = static_cast<uint8_t>(raw_difficulty);
    out.difficulty = difficulty(raw_difficulty);
    out.mission = static_cast<int>(progress);
    out.continues = read<uint16_t>(data, 0x158);
    out.play_time_seconds = read<uint32_t>(data, 0x168) / kFramesPerSecond;
    out.alerts = read<uint16_t>(data, 0x16E);
    out.kills = read<uint16_t>(data, 0x178);
    out.special_items_mask = read<uint16_t>(data, 0x17A);
    out.special_item_used = out.special_items_mask != 0;
    out.cqc_chokes = out.cqc_holds = read<uint16_t>(data, 0x180);
    out.headshots = read<uint16_t>(data, 0x182);
    out.knife_defeats = read<uint16_t>(data, 0x184) + read<uint16_t>(data, 0x186);
    out.side_rolls = read<uint16_t>(data, 0x188);
    out.forward_rolls = read<uint16_t>(data, 0x18A);
    out.combat_highs = read<uint16_t>(data, 0x18C);
    out.pickups = read<uint16_t>(data, 0x18E) + read<uint16_t>(data, 0x190);
    out.hold_ups = read<uint16_t>(data, 0x192);
    out.body_searches = read<uint16_t>(data, 0x194);
    out.praises = read<uint16_t>(data, 0x196);
    out.items_given = read<uint16_t>(data, 0x198);
    out.syringe_uses = read<uint16_t>(data, 0x19A) + read<uint16_t>(data, 0x19C);
    out.magazine_pages = read<uint16_t>(data, 0x19E) + read<uint16_t>(data, 0x1A0);
    out.crouch_time_seconds = read<uint32_t>(data, 0x1A8) / kFramesPerSecond;
    out.crawl_time_seconds = read<uint32_t>(data, 0x1AC) / kFramesPerSecond;
    out.wall_time_seconds = read<uint32_t>(data, 0x1B4) / kFramesPerSecond;
    out.box_time_seconds = (read<uint32_t>(data, 0x1B8) + read<uint32_t>(data, 0x1BC))
        / kFramesPerSecond;
    out.rations_used = read<uint16_t>(data, 0xAE0);
    out.flashbacks_viewed = read<uint16_t>(data, 0x5A34);
    // Unique-flashback bit array at 0x5A49: 222 bits, the ACH_015 condition.
    // 0x5A34 counts every trigger, repeats included; this one does not.
    out.flashbacks_unique = 0;
    for (size_t index = 0; index < sizeof(out.flashback_bits); ++index) {
        const uint8_t byte = read<uint8_t>(data, kFlashbackFlagsOffset + index);
        out.flashback_bits[index] = byte;
        out.flashbacks_unique += std::popcount(static_cast<uint8_t>(
            byte & (index == kLastFlashbackByte ? kLastFlashbackByteMask : 0xFF)));
    }
    out.little_gray_claimed =
        (read<uint32_t>(data, kTitleFlagsOffset) & kLittleGrayClaimedBit) != 0;
    out.weapons_acquired = 0;
    for (size_t id = kFirstWeaponId; id <= kLastWeaponId; ++id) {
        if (id == kUnusedWeaponId) continue;
        const uint16_t state = read<uint16_t>(data, kWeaponStatesOffset + id * sizeof(uint16_t));
        out.weapons_acquired += state == 1 || state == 2;
    }
    return g_run.update(out, run_state({stage, stage_length}));
}

} // namespace bb::mgs4

void bb::asi_main()
{
    start_overlay(BB_GAME_LABEL, &mgs4::poll_stats, L"mgs4.exe", Game::MGS4);
}
