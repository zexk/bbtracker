#include "names.h"

#include <windows.h>

#include <atomic>
#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string_view>
#include <vector>

#include "../../common/log.h"
#include "../../common/mem.h"
#include "../../common/stats.h"
#include "../../overlay/overlay.h"
#include "../asi_main.h"

namespace bb::mgspw {

namespace {

// Community Cheat Engine anchors (mgspw-snake-swiss-v3.CT, RedCode):
// each pattern locates the instruction referencing the global, then the
// rip-relative displacement is resolved to the global itself.
constexpr wchar_t kModuleName[] = L"METAL GEAR SOLID PEACE WALKER.exe";

constexpr uint8_t kSaveRootPat[] = {
    0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, 0x48, 0x05, 0x3C, 0xBD, 0x00, 0x00, 0xC3};
constexpr bool kSaveRootWild[std::size(kSaveRootPat)] = {
    false, false, false, true, true, true, true, false, false, false, false, false, false, false};
constexpr int kSaveRootDisp = 3;

constexpr uint8_t kCharArrayPat[] = {0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x05,
                                     0x00, 0x00, 0x00, 0x00, 0x48, 0x63, 0xD1, 0x48,
                                     0x8B, 0x0C, 0xD0};
constexpr bool kCharArrayWild[std::size(kCharArrayPat)] = {
    false, false, false, false, false, false, false, false,
    true,  true,  true,  true,  false, false, false, false,
    false, false, false};
constexpr int kCharArrayDisp = 8;

// Mission-start init: clears the current-mission-id global to -1 before the
// script variable is read into it, so the store's displacement names it.
constexpr uint8_t kMissionIdPat[] = {0x33, 0xDB, 0xBE, 0xFF, 0xFF, 0xFF, 0xFF,
                                     0xB9, 0xFF, 0xFF, 0xFF, 0x00, 0x48, 0x89,
                                     0x1D, 0x00, 0x00, 0x00, 0x00, 0x8B, 0xEB,
                                     0x89, 0x35, 0x00, 0x00, 0x00, 0x00};
constexpr bool kMissionIdWild[std::size(kMissionIdPat)] = {
    false, false, false, false, false, false, false,
    false, false, false, false, false, false, false,
    false, true,  true,  true,  true,  false, false,
    false, false, true,  true,  true,  true};
constexpr int kMissionIdDisp = 23;

constexpr uint8_t kMissionTimePat[] = {0x48, 0x89, 0x05, 0x00, 0x00, 0x00, 0x00,
                                       0x41, 0x0F, 0xBA, 0xE1, 0x19};
constexpr bool kMissionTimeWild[std::size(kMissionTimePat)] = {
    false, false, false, true, true, true, true, false, false, false, false, false};
constexpr int kMissionTimeDisp = 3;

// Save-block field offsets (hex, from CT comments/accessors).
constexpr size_t kStageOff = 0x54;
constexpr size_t kStageLen = 24;
constexpr size_t kTotalPlayOff = 0x84;
constexpr size_t kMissionPlayOff = 0x10;
constexpr size_t kResultTimeOff = 0x3C980;
constexpr size_t kWeaponArrayOff = 0xBD3C;
constexpr size_t kWeaponStride = 0x1C;
constexpr size_t kWeaponUseOff = 0x14;

// Character-array layout.
constexpr size_t kCharCount = 40;
constexpr size_t kHpOff = 0x11BE;     // u16, regenerates ~20/s
constexpr size_t kMaxHpOff = 0x11C0;  // u16, the deployed soldier's own maximum
constexpr int kNominalMaxHp = 8000;   // Snake's, and the damage-counter scale
constexpr size_t kWeaponIdOff = 0x14B8;

// Render thread reads both clocks after poll_stats resolves their anchors.
std::atomic_uintptr_t g_saveroot_ptr = 0;
std::atomic_uintptr_t g_mission_time = 0; // timer block: total +0x00, mission +0x10
uintptr_t g_chararray_ptr = 0;  // address holding character-pointer-array
uintptr_t g_mission_id = 0;     // address of current mission id (-1 outside a mission)
uintptr_t g_stat_array = 0;     // address holding the stat descriptor array pointer
uintptr_t g_local_player = 0;   // byte index into per-player mission tallies (0..8)
uintptr_t g_online_root = 0;    // address holding online subsystem base pointer
uintptr_t g_result_object = 0;  // address holding Player Data/result UI object
uintptr_t g_region_object = 0;  // address holding the region label object
bool g_scanned = false;
bool g_dumped = false;

// Lifetime-stat descriptors are one flat array, indexed directly by the low
// 16 bits of the stat id - the game's own getter (0x1400E3A90) does
// `record = *PW_STATARRAY - 0x10 + (id & 0xFFFF) * 0x28`. Records are 0x28
// bytes: +0x10 u32 id, +0x18 inline tally or player-tally pointer, +0x20 i32 career.
// The 999999 seen at +0x00 and +0x28 is one record's bound plus the next
// record's, which is what made these look like 48-byte records.
// The array reallocates between missions, so the pointer is re-read each poll
// and every record is checked against its expected id before use.
// Stat-descriptor getter (0x1400E3A90): sub rsp,0x58 / movsx rax,cx /
// lea rcx,[rax+rax*4] / mov rax,[rip+disp] / movups xmm0,[rax+rcx*8+0x10].
// The disp lands on the array pointer; +0x10 in that last operand is why the
// records start one bias below it.
constexpr uint8_t kStatArrayPat[] = {
    0x48, 0x83, 0xEC, 0x58, 0x48, 0x0F, 0xBF, 0xC1, 0x48, 0x8D, 0x0C, 0x80,
    0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x10, 0x44, 0xC8, 0x10};
constexpr bool kStatArrayWild[] = {
    false, false, false, false, false, false, false, false, false, false, false, false,
    false, false, false, true, true, true, true, false, false, false, false, false};

// Career commit selects this player's mission tally (0x1400E42A7).
constexpr uint8_t kLocalPlayerPat[] = {
    0x44, 0x0F, 0xB6, 0x15, 0, 0, 0, 0,
    0x48, 0x89, 0x5C, 0x24, 0x30, 0xBB, 0x20, 0x03, 0x00, 0x00};
constexpr bool kLocalPlayerWild[] = {
    false, false, false, false, true, true, true, true,
    false, false, false, false, false, false, false, false, false, false};

constexpr uint8_t kOnlineRootPat[] = {
    0x8B, 0xC1, 0x48, 0x69, 0xC8, 0x28, 0x35, 0x00, 0x00,
    0x48, 0x8B, 0x05, 0, 0, 0, 0, 0x48, 0x05, 0x08, 0x50, 0x00, 0x00};
constexpr bool kOnlineRootWild[] = {
    false, false, false, false, false, false, false, false, false,
    false, false, false, true, true, true, true, false, false, false, false, false, false};

constexpr uint8_t kResultObjectPat[] = {
    0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x8B, 0x05, 0, 0, 0, 0,
    0x48, 0x8B, 0x1D, 0, 0, 0, 0, 0x85, 0xC0, 0x75, 0x09};
constexpr bool kResultObjectWild[] = {
    false, false, false, false, false, false, false, false, true, true, true, true,
    false, false, false, true, true, true, true, false, false, false, false};

constexpr uint8_t kRegionObjectPat[] = {
    0x8B, 0x05, 0, 0, 0, 0, 0x48, 0x8B, 0x1D, 0, 0, 0, 0,
    0x85, 0xC0, 0x75, 0x09, 0x48, 0x85, 0xDB, 0x0F, 0x84,
    0x19, 0x01, 0x00, 0x00, 0x39, 0x43, 0x28};
constexpr bool kRegionObjectWild[] = {
    false, false, true, true, true, true, false, false, false, true, true, true, true,
    false, false, false, false, false, false, false, false, false,
    false, false, false, false, false, false, false};

constexpr uint32_t kStatMax = 999999;
constexpr size_t kStatScanSize = 0x30000;
constexpr size_t kStatStride = 0x28;
constexpr uintptr_t kStatArrayBias = 0x10;
constexpr uint32_t kStatIndexMax = 0x123;  // the getter's own bounds check

constexpr bool run_flush_stage(std::string_view stage)
{
    return stage.substr(0, 8) == "my_outer" || stage == "ms_lobby"
        || stage == "vs_lobby" || stage == "vs_result" || stage == "title"
        || stage == "r_title" || stage == "browser";
}

static_assert(run_flush_stage("my_outer_trade"));
static_assert(!run_flush_stage("result"));
static_assert(!run_flush_stage("w01s04a"));

constexpr bool valid_stage(std::string_view stage)
{
    for (char c : stage) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

static_assert(valid_stage("w01s04a"));
static_assert(valid_stage("my_outer_trade"));
static_assert(!valid_stage("w01[s04a"));

// Span covering every descriptor the indexed paths touch: family ids stop
// at the getter's bound above, and the 0xDD..0x110 codename axes sit below
// it. Validated once per poll instead of once per record.
constexpr size_t kStatRecordsSpan = (kStatIndexMax + 1) * kStatStride;

// The stat descriptor array reallocates between missions: re-resolve its
// records base each poll, or 0 when the pointer cell is unreadable.
uintptr_t stat_records()
{
    uintptr_t array = 0;
    if (!g_stat_array || !mem::copy(g_stat_array, array)) return 0;
    return array > kStatArrayBias ? array - kStatArrayBias : 0;
}
constexpr uint32_t kHeadshotIds[] = {0x4420031};
constexpr uint32_t kKillIds[] = {0x420008};
// Sleep/tranq takedowns only. Stun rod and CQC increment the separate
// stun total (0x442002D), not this counter.
constexpr uint32_t kTranqIds[] = {0x442002E};
constexpr uint32_t kPistolIds[] = {0x200F9};      // non-lethal, pistol
constexpr uint32_t kPistolLethalIds[] = {0x200DF}; // lethal, pistol
constexpr uint32_t kCqcIds[] = {0x20104};          // CQC takedowns, any variant
constexpr uint32_t kStunRodIds[] = {0x20105};      // stun rod knockouts
constexpr uint32_t kGrenadeIds[] = {0x200E6};      // lethal, grenade
constexpr uint32_t kRocketIds[] = {0x200E5};       // lethal, rocket launcher
constexpr uint32_t kPlacedIds[] = {0x200E8};       // lethal, placed explosive (C4)
// Kills on enemies that never spotted the player. Confirmed by a run the
// player reported as 2 stealth kills then 1 after being found: +2.
constexpr uint32_t kStealthKillIds[] = {0x2007C};
// Career damage taken, on the same 0-8000 scale as health: a run that ended
// with the player at 5070/8000 moved it by 2958.
constexpr uint32_t kDamageTakenIds[] = {0x20023};
constexpr uint32_t kArIds[] = {0x200E0};      // lethal, assault rifle
constexpr uint32_t kShotgunIds[] = {0x200E4}; // lethal, shotgun
constexpr uint32_t kSniperIds[] = {0x200E1};  // lethal, sniper rifle
constexpr uint32_t kLmgIds[] = {0x200E2};     // lethal, LMG
constexpr uint32_t kSniperNlIds[] = {0x200FB}; // non-lethal, sniper (Mosin)
constexpr uint32_t kFultonIds[] = {0x2008E};    // Fulton: enemies
constexpr uint32_t kPrisonerIds[] = {0x2008F};  // Fulton: prisoners
constexpr uint32_t kNoItemClearIds[] = {0x44200DC};  // "no recovery items used"
constexpr uint32_t kHoldUpIds[] = {0x4420030};       // "Total Hold-ups"
constexpr uint32_t kCqcUseIds[] = {0x442007B};       // "Total CQC Count"
constexpr uint32_t kHeroismIds[] = {0x4420077};
constexpr uint32_t kNoAlertClearIds[] = {0x442011E};
constexpr uint32_t kNoKillClearIds[] = {0x442011F};
constexpr uint32_t kAlertIds[] = {0x420002};
// Non-headshot kills: zero across every headshot-only run, +3 on a
// body-shot-only run with the same weapon. 0x2002F moves with it.
constexpr uint32_t kBodyKillIds[] = {0x200ED};

// Mirror 0x1400E3B60: +0x18 is inline for flag 0x40, otherwise a pointer
// to nine player tallies. The caller has already validated the descriptor.
int mission_stat(uintptr_t record, int player)
{
    const uint32_t flags = mem::read<uint32_t>(record + 0x10) >> 16;
    if (flags & 0x220) return -1;
    uintptr_t address = record + 0x18;
    if (!(flags & 0x40)) {
        if (player < 0 || player > 8) return -1;
        const uintptr_t pointer = mem::read<uintptr_t>(address);
        if (!pointer) return -1;
        address = pointer + player * sizeof(int32_t);
    }
    int value = -1;
    if ((flags & 0x40) != 0) {
        value = mem::read<int32_t>(address);
    } else if (!mem::copy(address, value)) {
        return -1;
    }
    return value >= 0 && value <= static_cast<int>(kStatMax) ? value : -1;
}

void read_stat_families(uintptr_t block, GameStats& out)
{
    struct Family {
        const uint32_t* ids;
        size_t count;
        int* field;
        int* mission_field;  // descriptor +0x18: this mission's tally
    };
    const Family families[] = {
        {kHeadshotIds, std::size(kHeadshotIds), &out.pw_headshots, &out.pw_m_headshots},
        {kKillIds, std::size(kKillIds), &out.pw_kills, &out.pw_m_kills},
        {kTranqIds, std::size(kTranqIds), &out.pw_tranq, &out.pw_m_tranq},
        {kAlertIds, std::size(kAlertIds), &out.pw_alerts, &out.pw_m_alerts},
        {kBodyKillIds, std::size(kBodyKillIds), &out.pw_body_kills, &out.pw_m_body_kills},
        {kPistolIds, std::size(kPistolIds), &out.pw_pistol_takedowns, nullptr},
        {kArIds, std::size(kArIds), &out.pw_ar_takedowns, nullptr},
        {kShotgunIds, std::size(kShotgunIds), &out.pw_shotgun_takedowns, nullptr},
        {kSniperIds, std::size(kSniperIds), &out.pw_sniper_takedowns, nullptr},
        {kLmgIds, std::size(kLmgIds), &out.pw_lmg_takedowns, nullptr},
        {kSniperNlIds, std::size(kSniperNlIds), &out.pw_sniper_nonlethal, nullptr},
        {kPistolLethalIds, std::size(kPistolLethalIds), &out.pw_pistol_lethal, nullptr},
        {kCqcIds, std::size(kCqcIds), &out.pw_cqc_takedowns, nullptr},
        {kStunRodIds, std::size(kStunRodIds), &out.pw_stun_rod_takedowns, &out.pw_m_stun_rod_takedowns},
        {kGrenadeIds, std::size(kGrenadeIds), &out.pw_grenade_takedowns, nullptr},
        {kRocketIds, std::size(kRocketIds), &out.pw_rocket_takedowns, nullptr},
        {kPlacedIds, std::size(kPlacedIds), &out.pw_placed_takedowns, nullptr},
        {kStealthKillIds, std::size(kStealthKillIds), &out.pw_stealth_kills, nullptr},
        {kDamageTakenIds, std::size(kDamageTakenIds), &out.pw_damage_taken, nullptr},
        {kFultonIds, std::size(kFultonIds), &out.pw_fulton_recoveries, nullptr},
        {kPrisonerIds, std::size(kPrisonerIds), &out.pw_prisoner_extractions, nullptr},
        {kNoItemClearIds, std::size(kNoItemClearIds), &out.pw_noitem_clears, nullptr},
        {kHoldUpIds, std::size(kHoldUpIds), &out.pw_holdups, &out.pw_m_holdups},
        {kCqcUseIds, std::size(kCqcUseIds), &out.pw_cqc_uses, &out.pw_m_cqc_uses},
        {kHeroismIds, std::size(kHeroismIds), &out.pw_heroism, &out.pw_m_heroism},
        {kNoAlertClearIds, std::size(kNoAlertClearIds), &out.pw_noalert_clears, nullptr},
        {kNoKillClearIds, std::size(kNoKillClearIds), &out.pw_nokill_clears, nullptr},
    };
    for (const Family& f : families) {
        *f.field = -1;
        if (f.mission_field) {
            *f.mission_field = -1;
        }
    }
    uint8_t local_player = 0xFF;
    const int player = g_local_player && mem::copy(g_local_player, local_player)
        ? local_player : -1;
    // Direct index, the way the game's own getter does it. Falls back to the
    // linear scan below if the array pointer did not resolve.
    const uintptr_t records = stat_records();
    if (records) {
        // One validation for the whole descriptor span; the per-record id
        // check below still guards against layout changes, and out-of-span
        // indices keep their individual skip above.
        static std::vector<uint8_t> snapshot(kStatRecordsSpan);
        const bool span_ok = mem::copy(records, snapshot.data(), snapshot.size());
        bool all_ok = true;
        for (const Family& f : families) {
            for (size_t i = 0; i < f.count; ++i) {
                const uint32_t index = f.ids[i] & 0xFFFF;
                if (index > kStatIndexMax) {
                    continue;
                }
                if (!span_ok) {
                    all_ok = false;
                    continue;
                }
                const uintptr_t rec = reinterpret_cast<uintptr_t>(snapshot.data())
                    + index * kStatStride;
                const auto* words = reinterpret_cast<const uint32_t*>(rec);
                // The index is derived from the id, so a record whose id
                // does not match means the array moved or the layout
                // changed - never trust the value in that case.
                if (words[4] != f.ids[i]) {
                    all_ok = false;
                    continue;
                }
                *f.field = static_cast<int>(words[8]);
                if (f.mission_field) *f.mission_field = mission_stat(rec, player);
            }
        }
        if (all_ok) {
            return;
        }
    }
    // One linear pass; readability checked per page (the table moves, but
    // pages are cheap to test). Max across id matches: the live copy leads
    // stale snapshot copies.
    for (size_t page = 0; page < kStatScanSize; page += 0x1000) {
        const uintptr_t base = block + page;
        const size_t span = page + 0x1000 <= kStatScanSize + 44
            ? 0x1000
            : kStatScanSize + 44 - page;
        std::array<uint8_t, 0x1000> snapshot{};
        if (!mem::copy(base, snapshot.data(), span)) continue;
        for (size_t off = 0; off + 48 <= span; off += 4) {
            const uintptr_t rec_address = reinterpret_cast<uintptr_t>(snapshot.data()) + off;
            const auto* rec = reinterpret_cast<const uint32_t*>(rec_address);
            if (rec[0] != kStatMax || rec[10] != kStatMax) {
                continue;
            }
            const uint32_t id = rec[4];
            const int value = static_cast<int>(rec[8]);
            for (const Family& f : families) {
                for (size_t i = 0; i < f.count; ++i) {
                    if (id != f.ids[i]) {
                        continue;
                    }
                    if (value > *f.field) {
                        *f.field = value;
                    }
                    if (f.mission_field) {
                        const int mission = mission_stat(rec_address, player);
                        if (mission > *f.mission_field) *f.mission_field = mission;
                    }
                }
            }
        }
    }
}

// save+0x1BFF0 + id, ids 1..24; see 0x1405448C0 (grade) and 0x140544B20 (grant).
constexpr uintptr_t kCodenameStateOff = 0x1BFF0;

void read_codename_state(uintptr_t block, GameStats& out)
{
    // Precondition: block passed the save-span validation in poll_stats.
    const uintptr_t base = block + kCodenameStateOff;
    for (size_t id = 1; id < std::size(out.pw_codename_state); ++id) {
        out.pw_codename_state[id] =
            mem::read<uint8_t>(base + id);
    }
    out.pw_codename_state_ok = true;
}

// save+0x1C009 + index, index 1..110; granter 0x140544B80, bit 0 is ownership.
constexpr uintptr_t kInsigniaStateOff = 0x1C009;
constexpr size_t kInsigniaCount = 110;

void read_insignia_state(uintptr_t block, GameStats& out)
{
    // Precondition: block passed the save-span validation in poll_stats.
    const uintptr_t base = block + kInsigniaStateOff;
    int owned = 0;
    for (size_t index = 1; index <= kInsigniaCount; ++index) {
        owned += mem::read<uint8_t>(base + index) & 1;
    }
    out.pw_insignias = owned;
}

void read_codename_axes(GameStats& out)
{
    out.pw_codename_axes_ok = false;
    const uintptr_t records = stat_records();
    if (!records) return;
    // One validation for the whole axes span (a subset of the descriptor
    // span above); per-record checks stay as the fallback.
    static std::vector<uint8_t> snapshot(kStatRecordsSpan);
    if (!mem::copy(records, snapshot.data(), snapshot.size())) return;
    const uintptr_t base = reinterpret_cast<uintptr_t>(snapshot.data());
    for (int axis = 0; axis < 4; ++axis) {
        for (int slot = 0; slot < 12; ++slot) {
            const uint32_t index = 0xDD + axis * 13 + slot;
            const uintptr_t rec = base + index * kStatStride;
            if (mem::read<uint32_t>(rec + 0x10) != (0x20000u | index)) return;
            const int value = mem::read<int32_t>(rec + 0x20);
            if (value < 0 || value > static_cast<int>(kStatMax)) return;
            out.pw_codename_axes[axis][slot] = value;
        }
    }
    out.pw_codename_axes_ok = true;
}

bool pat_match(const uint8_t* p, const uint8_t* pat, const bool* wild, size_t n, size_t avail)
{
    if (avail < n) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        if (!wild[i] && p[i] != pat[i]) {
            return false;
        }
    }
    return true;
}

uintptr_t scan_one(HMODULE mod, const uint8_t* pat, const bool* wild, size_t n, int disp_off)
{
    uintptr_t target = 0;
    mem::for_each_code_region(mod, [&](uintptr_t begin, uintptr_t stop) {
        uintptr_t p = begin;
        while (p + n <= stop) {
            const auto* found = static_cast<const uint8_t*>(
                std::memchr(reinterpret_cast<const void*>(p), pat[0], stop - p - n + 1));
            if (!found) {
                break;
            }
            p = reinterpret_cast<uintptr_t>(found);
            if (!pat_match(found, pat, wild, n, stop - p)) {
                ++p;
                continue;
            }
            const int32_t disp = *reinterpret_cast<volatile const int32_t*>(p + disp_off);
            target = static_cast<uintptr_t>(static_cast<int64_t>(p) + disp_off + 4 + disp);
            return true;
        }
        return false;
    });
    return target;
}

void ensure_resolved()
{
    if (g_scanned) {
        return;
    }
    g_scanned = true;
    HMODULE mod = GetModuleHandleW(kModuleName);
    if (!mod) {
        mod = GetModuleHandleW(nullptr);
    }
    if (!mod) {
        LOG_WARN("MGSPW module not found for signature scan");
        return;
    }
    const auto base = reinterpret_cast<uintptr_t>(mod);
    g_saveroot_ptr = scan_one(mod, kSaveRootPat, kSaveRootWild,
                              std::size(kSaveRootPat), kSaveRootDisp);
    g_mission_time = scan_one(mod, kMissionTimePat, kMissionTimeWild,
                              std::size(kMissionTimePat), kMissionTimeDisp);
    g_chararray_ptr = scan_one(mod, kCharArrayPat, kCharArrayWild,
                               std::size(kCharArrayPat), kCharArrayDisp);
    g_stat_array = scan_one(mod, kStatArrayPat, kStatArrayWild,
                            std::size(kStatArrayPat), 15);
    g_local_player = scan_one(mod, kLocalPlayerPat, kLocalPlayerWild,
                              std::size(kLocalPlayerPat), 4);
    g_online_root = scan_one(mod, kOnlineRootPat, kOnlineRootWild,
                             std::size(kOnlineRootPat), 12);
    g_result_object = scan_one(mod, kResultObjectPat, kResultObjectWild,
                               std::size(kResultObjectPat), 15);
    g_region_object = scan_one(mod, kRegionObjectPat, kRegionObjectWild,
                               std::size(kRegionObjectPat), 9);
    g_mission_id = scan_one(mod, kMissionIdPat, kMissionIdWild,
                            std::size(kMissionIdPat), kMissionIdDisp);
    LOG_INFO("MGSPW resolved save=%llX time=%llX chars=%llX",
             static_cast<unsigned long long>(g_saveroot_ptr ? g_saveroot_ptr - base : 0),
             static_cast<unsigned long long>(g_mission_time ? g_mission_time - base : 0),
             static_cast<unsigned long long>(g_chararray_ptr ? g_chararray_ptr - base : 0));
    if (!g_saveroot_ptr) {
        LOG_WARN("MGSPW PW_SAVEROOT pattern not found");
    }
    if (!g_mission_time) {
        LOG_WARN("MGSPW PW_MISSIONTIME pattern not found");
    }
    if (!g_chararray_ptr) {
        LOG_WARN("MGSPW PW_CHARARRAY pattern not found");
    }
    if (!g_stat_array) {
        LOG_WARN("MGSPW PW_STATARRAY pattern not found; falling back to the id scan");
    }
    if (!g_online_root) LOG_WARN("MGSPW online-player table pattern not found");
    if (!g_local_player) LOG_WARN("MGSPW local-player index pattern not found");
    if (!g_result_object) LOG_WARN("MGSPW result object pattern not found");
    if (!g_region_object) LOG_WARN("MGSPW region object pattern not found");
}

} // namespace

// Highest save-relative read is the insignia state; the stage string, play
// tallies, heroism, rank arrays, weapons and codename state all sit below
// it, so one validation covers the whole block per poll.
constexpr size_t kSaveBlockSpan = kInsigniaStateOff + kInsigniaCount + 1;

bool poll_mission_clock(uint32_t& ticks)
{
    const uintptr_t mission_time = g_mission_time.load();
    return mission_time && mem::copy(mission_time + kMissionPlayOff, ticks);
}

bool poll_stats(GameStats& out)
{
    out = {};
    ensure_resolved();
    const uintptr_t mission_time = g_mission_time.load();
    if (!g_saveroot_ptr && !mission_time && !g_chararray_ptr) {
        return false;
    }

    bool any = false;

    // The game resolves stage/mission state to a region; reuse that result.
    // Objects are replaced on load and absent in menus. Match the game's
    // handle/self-pointer checks before accepting the region index.
    std::array<uint8_t, 16> region_slot{};
    if (g_region_object >= 8
        && mem::copy(g_region_object - 8, region_slot.data(), region_slot.size())) {
        const auto object = mem::read<uintptr_t>(region_slot.data(), 8);
        const auto handle = mem::read<uint32_t>(region_slot.data(), 0);
        std::array<uint8_t, 0x114> region{};
        if (object && mem::copy(object, region.data(), region.size())
            && mem::read<uint32_t>(region.data(), 0x28) == handle
            && mem::read<uintptr_t>(region.data(), 0x30) == object) {
            out.pw_region_id = region_id(mem::read<int32_t>(region.data(), 0x110));
        }
    }

    std::array<uint8_t, 0x18> mission_clock{};
    if (mission_time && mem::copy(mission_time, mission_clock.data(), mission_clock.size())) {
        out.pw_mission_raw = mem::read<uint64_t>(mission_clock.data(), 0);
        out.pw_mission_play = mem::read<uint32_t>(mission_clock.data(), kMissionPlayOff);
        // Measured: raw ticks 300/s of active game time (a +42600 delta over
        // an interval where total play advanced exactly +142s). 3.33ms
        // resolution still breaks same-second best-time ties.
        out.play_time_seconds = static_cast<double>(out.pw_mission_raw) / 300.0;
        any = true;
        if (!g_dumped) {
            g_dumped = true;
            LOG_INFO("MGSPW mission timer block:");
            std::array<uint8_t, 0x60> dump{};
            if (mission_time >= 0x20 && mem::copy(mission_time - 0x20, dump.data(), dump.size())) {
                log_hex_dump(dump.data(), dump.size());
            }
        }
    }

    uintptr_t save_block = 0;
    const uintptr_t saveroot_ptr = g_saveroot_ptr.load();
    if (saveroot_ptr) mem::copy(saveroot_ptr, save_block);
    // One validation for the whole save block: every save-relative read
    // below lands inside this span, so the per-field checks collapse here.
    // A partially-mapped block reads as absent, the way each failed check
    // below used to leave its fields at their init values.
    static std::vector<uint8_t> save(kSaveBlockSpan);
    if (save_block && mem::copy(save_block, save.data(), save.size())) {
        const uintptr_t saved = reinterpret_cast<uintptr_t>(save.data());
        char stage[32]{};
        std::memcpy(stage, save.data() + kStageOff, kStageLen);
        stage[sizeof(stage) - 1] = '\0';
        if (!valid_stage(stage)) stage[0] = '\0';
        std::memcpy(out.pw_stage, stage, sizeof(out.pw_stage));
        std::memcpy(out.area_code, stage, sizeof(out.area_code) - 1);
        any = true;
        out.pw_total_play = mem::read<uint32_t>(save.data(), kTotalPlayOff);
        out.pw_result_time = mem::read<uint32_t>(save.data(), kResultTimeOff);
        constexpr size_t kHeroismOff = 0x64F4;
        constexpr size_t kHeroismDeltaOff = 0x64EC;
        constexpr size_t kGmpOff = 0xB52C;
        constexpr size_t kClearsOff = 0x656C;
        out.pw_heroism_delta = mem::read<int32_t>(save.data(), kHeroismDeltaOff);
        out.pw_heroism = mem::read<int32_t>(save.data(), kHeroismOff);
        out.pw_gmp = mem::read<uint32_t>(save.data(), kGmpOff);
        out.pw_clears = mem::read<int32_t>(save.data(), kClearsOff);
        // Per-mission rank array (u16 by mission id; 0 = S, 0xFFFF = never
        // cleared). Ids past the live list read as zeros, so stop at the
        // length that matches the confirmed clear/S counts.
        constexpr size_t kRankArrayOff = 0x32B4;
        constexpr size_t kRankArrayLen = 272;
        constexpr size_t kBestTimeOff = 0x29B4;
        const auto* ranks = reinterpret_cast<const uint16_t*>(save.data() + kRankArrayOff);
        int cleared = 0;
        int s_missions = 0;
        for (size_t i = 0; i < kRankArrayLen; ++i) {
            const uint16_t r = ranks[i];
            if (r == 0xFFFF) {
                continue;
            }
            ++cleared;
            if (r == 0) {
                ++s_missions;
            }
        }
        out.pw_unique_cleared = cleared;
        out.pw_s_missions = s_missions;
        // Current mission: the id indexes both per-mission arrays (subsets
        // of the validated span), so the overlay can show this mission's
        // stored rank and best time.
        int id = -1;
        if (g_mission_id && mem::copy(g_mission_id, id)) {
            out.pw_mission_id = id;
            if (id > 0 && static_cast<size_t>(id) < kRankArrayLen) {
                const uint16_t rank = mem::read<uint16_t>(
                    save.data(), kRankArrayOff + id * 2);
                const uint32_t best = mem::read<uint32_t>(
                    save.data(), kBestTimeOff + id * 4);
                out.pw_cur_rank = rank == 0xFFFF ? -1 : static_cast<int>(rank);
                out.pw_cur_best = best == 0xFFFFFFFFu ? 0 : best;
            } else {
                out.pw_cur_rank = -1;
                out.pw_cur_best = 0;
            }
        }
        read_stat_families(save_block, out);
        read_codename_axes(out);
        read_codename_state(saved, out);
        read_insignia_state(saved, out);
        static uintptr_t last_dump_block = 0;
        if (save_block != last_dump_block) {
            last_dump_block = save_block;
            LOG_INFO("MGSPW save header block %p:", reinterpret_cast<const void*>(save_block));
            log_hex_dump(save.data() + 0x40, 0x60);
        }
        for (int i = 0; i < 16; ++i) {
            const uint16_t use = mem::read<uint16_t>(
                save.data(), kWeaponArrayOff + i * kWeaponStride + kWeaponUseOff);
            out.pw_weapon_use[i] = static_cast<int>(use);
        }
    }

    uintptr_t root = 0;
    if (g_online_root && mem::copy(g_online_root, root)) {
        const uintptr_t table = root ? root + 0x5008 : 0;
        int count = -1;
        if (table && mem::copy(table, count)) {
            if (count >= 0 && count <= 64) {
                static std::vector<uint8_t> players;
                players.resize(0x110 * static_cast<size_t>(count + 1));
                if (mem::copy(table, players.data(), players.size())) {
                    out.pw_camaraderie = 0;
                    for (int i = 0; i < count; ++i) {
                        out.pw_camaraderie += mem::read<int32_t>(
                            players.data(), 0x110 * static_cast<size_t>(i + 1));
                    }
                }
            }
        }
    }

    uintptr_t object = 0;
    if (g_result_object && mem::copy(g_result_object, object)) {
        constexpr size_t kResultOff = 0x43D0;
        std::array<uint8_t, 0x2B> result{};
        if (object && mem::copy(object + kResultOff, result.data(), result.size())) {
            out.pw_codename_missions_required = mem::read<int32_t>(result.data(), 0x20);
            out.pw_codename_missions_counted = mem::read<int32_t>(result.data(), 0x24);
            out.pw_codename_grade5_ok = mem::read<uint8_t>(result.data(), 0x28) != 0;
            out.pw_codename_grade4_ok = mem::read<uint8_t>(result.data(), 0x29) != 0;
            out.pw_codename_result_ok = mem::read<uint8_t>(result.data(), 0x2A) != 0;
        }
    }

    uintptr_t arr = 0;
    if (g_chararray_ptr && mem::copy(g_chararray_ptr, arr)) {
        std::array<uintptr_t, kCharCount> characters{};
        if (arr && mem::copy(arr, characters.data(), sizeof(characters))) {
            const uintptr_t player = characters[0];
            if (player) {
                std::array<uint8_t, 4> health{};
                if (mem::copy(player + kHpOff, health.data(), health.size())) {
                    out.pw_player_hp = static_cast<int>(mem::read<int16_t>(health.data(), 0));
                    out.pw_player_max_hp = static_cast<int>(mem::read<uint16_t>(health.data(), 2));
                    out.current_health = out.pw_player_hp;
                    out.max_health = out.pw_player_max_hp > 0 ? out.pw_player_max_hp
                                                             : kNominalMaxHp;
                }
                int16_t weapon = -1;
                if (mem::copy(player + kWeaponIdOff, weapon)) out.pw_weapon_id = weapon;
                any = true;
            }
        }
    }

    // Region object disappears during cutscenes, loads and results. Latch run
    // display across those gaps; clear only after a confirmed hub/menu stage.
    const bool gameplay = g_region_object
        ? out.pw_region_id >= 0
        : (out.pw_stage[0] == 'w' && out.pw_stage[1] >= '0' && out.pw_stage[1] <= '9');
    static bool run_visible = false;
    if (gameplay) run_visible = true;
    else if (run_flush_stage(out.pw_stage)) run_visible = false;
    out.pw_in_mission = run_visible;

    // Per-sortie segment: latch career baselines whenever the stage
    // string changes. Careers land at results tally (actions) or lobby
    // exit (heroism/XP/GMP), so segment deltas appear then, not live
    // mid-mission. Unknown (-1) inputs latch as zero deltas until both
    // sides resolve.
    static GameStats seg_base{};
    static char seg_last_stage[32]{};
    static bool seg_have_base = false;
    if (!seg_have_base || std::strcmp(out.pw_stage, seg_last_stage) != 0) {
        seg_base = out;
        std::memcpy(seg_last_stage, out.pw_stage, sizeof(seg_last_stage));
        seg_have_base = true;
        if (out.pw_stage[0]) {
            LOG_INFO("MGSPW segment: %s", out.pw_stage);
        }
    }
    const auto seg_delta = [](int cur, int base) {
        return (cur < 0 || base < 0) ? 0 : cur - base;
    };
    out.seg_headshots = seg_delta(out.pw_headshots, seg_base.pw_headshots);
    out.seg_kills = seg_delta(out.pw_kills, seg_base.pw_kills);
    out.seg_tranq = seg_delta(out.pw_tranq, seg_base.pw_tranq);
    out.seg_heroism = out.pw_heroism - seg_base.pw_heroism;

    return any;
}

} // namespace bb::mgspw

void bb::asi_main()
{
    start_overlay(BB_GAME_LABEL, &mgspw::poll_stats,
                  L"METAL GEAR SOLID PEACE WALKER.exe", Game::MGSPW,
                  &mgspw::poll_mission_clock);
}
