#include "probe.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string_view>

#include "../../common/log.h"
#include "../../common/mem.h"

namespace bb::mgs4 {

using bb::mem::readable;
using bb::mem::read;

namespace {

constexpr uintptr_t kLinkvarbufPointer = 0x1C28B28;
constexpr size_t kLinkvarbufSize = 0x8344;

Difficulty difficulty(uint16_t value)
{
    switch (value) {
    case 20: return Difficulty::VeryEasy;
    case 30: return Difficulty::Easy;
    case 35: return Difficulty::Normal;
    case 40: return Difficulty::Hard;
    default: return Difficulty::Extreme;
    }
}

constexpr bool ranked_stage(std::string_view stage)
{
    return stage.size() == 7 && stage[0] == 's'
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

} // namespace

bool poll_stats(GameStats& out)
{
    const auto module = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (!module || !readable(module + kLinkvarbufPointer, sizeof(uintptr_t))) return false;
    const uintptr_t address = *reinterpret_cast<volatile const uintptr_t*>(module + kLinkvarbufPointer);
    if (!address || !readable(address, kLinkvarbufSize)) return false;
    const auto* data = reinterpret_cast<const uint8_t*>(address);

    const uint16_t raw_difficulty = read<uint16_t>(data, 0x006);
    const uint32_t progress = read<uint32_t>(data, 0x054);
    if ((raw_difficulty != 20 && raw_difficulty != 30 && raw_difficulty != 35
         && raw_difficulty != 40 && raw_difficulty != 50) || progress > 291) return false;

    static uintptr_t last_address = 0;
    if (address != last_address) {
        LOG_INFO("MGS4 linkvarbuf %p (module+0x%llX)", reinterpret_cast<const void*>(address),
                 static_cast<unsigned long long>(kLinkvarbufPointer));
        last_address = address;
    }

    out = {};
    char stage[8]{};
    std::memcpy(stage, data + 0x034, 7);
    const size_t stage_length = std::char_traits<char>::length(stage);
    if (!ranked_stage({stage, stage_length})) return false;
    static char last_stage[8]{};
    if (std::strcmp(stage, last_stage) != 0) {
        LOG_INFO("MGS4 area: %s", stage);
        std::memcpy(last_stage, stage, sizeof(stage));
    }
    std::memcpy(out.area_code, stage, sizeof(stage));
    out.difficulty_raw = static_cast<uint8_t>(raw_difficulty);
    out.difficulty_game_byte = static_cast<uint8_t>(raw_difficulty);
    out.difficulty = difficulty(raw_difficulty);
    out.mission = static_cast<int>(progress);
    out.continues = read<uint16_t>(data, 0x158);
    out.play_time_seconds = read<uint32_t>(data, 0x168) / 60.0;
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
    out.crouch_time_seconds = read<uint32_t>(data, 0x1A8) / 60.0;
    out.crawl_time_seconds = read<uint32_t>(data, 0x1AC) / 60.0;
    out.wall_time_seconds = read<uint32_t>(data, 0x1B4) / 60.0;
    out.box_time_seconds = (read<uint32_t>(data, 0x1B8) + read<uint32_t>(data, 0x1BC)) / 60.0;
    out.rations_used = read<uint16_t>(data, 0xAE0);
    out.flashbacks_viewed = read<uint16_t>(data, 0x5A34);
    out.weapons_acquired = 0;
    for (size_t id = 1; id <= 73; ++id) {
        if (id == 11) continue;
        const uint16_t state = read<uint16_t>(data, 0x1D4 + id * 2);
        out.weapons_acquired += state == 1 || state == 2;
    }
    return true;
}

} // namespace bb::mgs4
