#include "probe.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <iterator>

#include "../../common/log.h"
#include "../../common/mem.h"

namespace bb::mgspw {

using bb::mem::range_readable;

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
constexpr size_t kStagePlayOff = 0x88;
constexpr size_t kWeaponArrayOff = 0xBD3C;
constexpr size_t kWeaponStride = 0x1C;
constexpr size_t kWeaponUseOff = 0x14;

// Character-array layout.
constexpr size_t kCharCount = 40;
constexpr size_t kHpOff = 0x11BE;  // u16, 8000 = full; regenerates ~20/s
constexpr size_t kWeaponIdOff = 0x14B8;

uintptr_t g_saveroot_ptr = 0;   // address holding save-block pointer
uintptr_t g_mission_time = 0;   // address of qword elapsed timer
uintptr_t g_chararray_ptr = 0;  // address holding character-pointer-array
uintptr_t g_mission_id = 0;     // address of current mission id (-1 outside a mission)
bool g_scanned = false;
bool g_dumped = false;

// Lifetime-stat descriptor: 48-byte records framing {max,max} around the
// id, with the value 32 bytes past the record start:
//   +0x00 u32 max (999999) ... +0x10 u32 id, +0x18 i32 last-delta,
//   +0x20 i32 value, +0x28 u32 max. The table reallocates between
// missions, so resolve by id on every poll (cheap linear scan).
constexpr uint32_t kStatMax = 999999;
constexpr size_t kStatScanSize = 0x30000;
constexpr uint32_t kHeadshotIds[] = {0x4420031};
constexpr uint32_t kKillIds[] = {0x420008};
// Non-lethal takedown total. 0x200F9 is the pistol-only subset, so it is
// tracked separately: on a 7-takedown run (6 pistol, 1 CQC) the total moved
// +7 and the pistol counter +6.
constexpr uint32_t kTranqIds[] = {0x442002E};
constexpr uint32_t kPistolIds[] = {0x200F9};      // non-lethal, pistol
constexpr uint32_t kPistolLethalIds[] = {0x200DF}; // lethal, pistol
constexpr uint32_t kCqcIds[] = {0x20104};          // CQC takedowns, any variant
constexpr uint32_t kGrenadeIds[] = {0x200E6};      // lethal, grenade
constexpr uint32_t kRocketIds[] = {0x200E5};       // lethal, rocket launcher
constexpr uint32_t kPlacedIds[] = {0x200E8};       // lethal, placed explosive (C4)
// Kills on enemies that never spotted the player. Confirmed by a run the
// player reported as 2 stealth kills then 1 after being found: +2.
constexpr uint32_t kStealthKillIds[] = {0x2007C};
constexpr uint32_t kArIds[] = {0x200E0};      // lethal, assault rifle
constexpr uint32_t kShotgunIds[] = {0x200E4}; // lethal, shotgun
constexpr uint32_t kSniperIds[] = {0x200E1};  // lethal, sniper rifle
constexpr uint32_t kLmgIds[] = {0x200E2};     // lethal, LMG
constexpr uint32_t kSniperNlIds[] = {0x200FB}; // non-lethal, sniper (Mosin)
constexpr uint32_t kFultonIds[] = {0x2008E};    // Fulton: enemies
constexpr uint32_t kPrisonerIds[] = {0x2008F};  // Fulton: prisoners
constexpr uint32_t kNoItemClearIds[] = {0x44200DC};  // "no recovery items used"
constexpr uint32_t kHoldUpIds[] = {0x4420030};       // "Total Hold-ups"
constexpr uint32_t kNoAlertClearIds[] = {0x442011E};
constexpr uint32_t kNoKillClearIds[] = {0x442011F};
constexpr uint32_t kAlertIds[] = {0x420002};
// Non-headshot kills: zero across every headshot-only run, +3 on a
// body-shot-only run with the same weapon. 0x2002F moves with it.
constexpr uint32_t kBodyKillIds[] = {0x200ED};

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
        {kGrenadeIds, std::size(kGrenadeIds), &out.pw_grenade_takedowns, nullptr},
        {kRocketIds, std::size(kRocketIds), &out.pw_rocket_takedowns, nullptr},
        {kPlacedIds, std::size(kPlacedIds), &out.pw_placed_takedowns, nullptr},
        {kStealthKillIds, std::size(kStealthKillIds), &out.pw_stealth_kills, nullptr},
        {kFultonIds, std::size(kFultonIds), &out.pw_fulton_recoveries, nullptr},
        {kPrisonerIds, std::size(kPrisonerIds), &out.pw_prisoner_extractions, nullptr},
        {kNoItemClearIds, std::size(kNoItemClearIds), &out.pw_noitem_clears, nullptr},
        {kHoldUpIds, std::size(kHoldUpIds), &out.pw_holdups, nullptr},
        {kNoAlertClearIds, std::size(kNoAlertClearIds), &out.pw_noalert_clears, nullptr},
        {kNoKillClearIds, std::size(kNoKillClearIds), &out.pw_nokill_clears, nullptr},
    };
    for (const Family& f : families) {
        *f.field = -1;
        if (f.mission_field) {
            *f.mission_field = -1;
        }
    }
    // One linear pass; readability checked per page (the table moves, but
    // pages are cheap to test). Max across id matches: the live copy leads
    // stale snapshot copies.
    for (size_t page = 0; page < kStatScanSize; page += 0x1000) {
        const uintptr_t base = block + page;
        if (!range_readable(base, 0x1000)) {
            continue;
        }
        const size_t span = page + 0x1000 <= kStatScanSize + 44
            ? 0x1000
            : kStatScanSize + 44 - page;
        for (size_t off = 0; off + 48 <= span; off += 4) {
            const auto* rec = reinterpret_cast<volatile const uint32_t*>(base + off);
            if (rec[0] != kStatMax || rec[10] != kStatMax) {
                continue;
            }
            const uint32_t id = rec[4];
            const int value = static_cast<int>(rec[8]);
            const int mission = static_cast<int>(rec[6]);
            for (const Family& f : families) {
                for (size_t i = 0; i < f.count; ++i) {
                    if (id != f.ids[i]) {
                        continue;
                    }
                    if (value > *f.field) {
                        *f.field = value;
                    }
                    // Only the 0x042/0x442 families keep a real tally here;
                    // the 0x002 copies leave junk in the slot.
                    if (f.mission_field && mission >= 0 && mission < 10000
                        && mission > *f.mission_field) {
                        *f.mission_field = mission;
                    }
                }
            }
        }
    }
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
    const auto base = reinterpret_cast<uintptr_t>(mod);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(mod);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return 0;
    }
    const auto* nt =
        reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return 0;
    }
    const uintptr_t scan_start = base + nt->OptionalHeader.BaseOfCode;
    const size_t scan_size = nt->OptionalHeader.SizeOfCode;
    const uintptr_t scan_end = scan_start + scan_size;

    for (uintptr_t addr = scan_start; addr < scan_end; addr += 0x1000) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))
            || mbi.State != MEM_COMMIT) {
            continue;
        }
        const uintptr_t region_end =
            reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        const uintptr_t stop = region_end < scan_end ? region_end : scan_end;
        uintptr_t p = addr > reinterpret_cast<uintptr_t>(mbi.BaseAddress)
            ? addr
            : reinterpret_cast<uintptr_t>(mbi.BaseAddress);
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
            const uintptr_t target =
                static_cast<uintptr_t>(static_cast<int64_t>(p) + disp_off + 4 + disp);
            return target;
        }
        if (region_end > addr + 0x1000) {
            addr = region_end - 0x1000;
        }
    }
    return 0;
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
}

} // namespace

bool poll_stats(GameStats& out)
{
    out = {};
    ensure_resolved();
    if (!g_saveroot_ptr && !g_mission_time && !g_chararray_ptr) {
        return false;
    }

    bool any = false;

    if (g_mission_time && range_readable(g_mission_time, 0x18)) {
        out.pw_mission_raw =
            *reinterpret_cast<volatile const uint64_t*>(g_mission_time);
        out.pw_mission_aux =
            *reinterpret_cast<volatile const uint64_t*>(g_mission_time + 0x08);
        out.pw_area =
            *reinterpret_cast<volatile const uint32_t*>(g_mission_time + 0x10);
        out.pw_mission_secondary =
            *reinterpret_cast<volatile const uint32_t*>(g_mission_time + 0x14);
        out.pw_mission_ok = true;
        // Measured: raw ticks 300/s of active game time (a +42600 delta over
        // an interval where total play advanced exactly +142s). 3.33ms
        // resolution still breaks same-second best-time ties.
        out.play_time_seconds = static_cast<double>(out.pw_mission_raw) / 300.0;
        any = true;
        if (!g_dumped) {
            g_dumped = true;
            LOG_INFO("MGSPW mission timer block:");
            log_hex_dump(reinterpret_cast<const uint8_t*>(g_mission_time - 0x20), 0x60);
        }
    }

    uintptr_t save_block = 0;
    if (g_saveroot_ptr && range_readable(g_saveroot_ptr, sizeof(uintptr_t))) {
        save_block = *reinterpret_cast<volatile const uintptr_t*>(g_saveroot_ptr);
    }
    if (save_block) {
        out.pw_saveroot_ok = true;
        if (range_readable(save_block + kStageOff, kStageLen)) {
            char stage[32]{};
            std::memcpy(stage, reinterpret_cast<const void*>(save_block + kStageOff),
                        kStageLen);
            stage[sizeof(stage) - 1] = '\0';
            std::memcpy(out.pw_stage, stage, sizeof(out.pw_stage));
            std::memcpy(out.area_code, stage, sizeof(out.area_code) - 1);
            any = true;
        }
        if (range_readable(save_block + kTotalPlayOff, 8)) {
            out.pw_total_play =
                *reinterpret_cast<volatile const uint32_t*>(save_block + kTotalPlayOff);
            out.pw_stage_play =
                *reinterpret_cast<volatile const uint32_t*>(save_block + kStagePlayOff);
        }
        constexpr size_t kHeroismOff = 0x64F4;
        constexpr size_t kHeroismDeltaOff = 0x64EC;
        constexpr size_t kGmpOff = 0xB52C;
        constexpr size_t kLastBestAOff = 0x586C;
        constexpr size_t kLastBestBOff = 0x5874;
        constexpr size_t kClearsOff = 0x656C;
        constexpr size_t kFultonOff = 0x130;
        constexpr size_t kLastScoreOff = 0x278;
        if (range_readable(save_block + kHeroismDeltaOff, 8)) {
            out.pw_heroism_delta =
                *reinterpret_cast<volatile const int32_t*>(save_block + kHeroismDeltaOff);
            out.pw_heroism =
                *reinterpret_cast<volatile const int32_t*>(save_block + kHeroismOff);
        }
        if (range_readable(save_block + kGmpOff, 4)) {
            out.pw_gmp =
                *reinterpret_cast<volatile const uint32_t*>(save_block + kGmpOff);
        }
        if (range_readable(save_block + kLastBestAOff, 16)) {
            out.pw_last_best_a =
                *reinterpret_cast<volatile const uint32_t*>(save_block + kLastBestAOff);
            out.pw_last_best_b =
                *reinterpret_cast<volatile const uint32_t*>(save_block + kLastBestBOff);
        }
        if (range_readable(save_block + kClearsOff, 4)) {
            out.pw_clears =
                *reinterpret_cast<volatile const int32_t*>(save_block + kClearsOff);
        }
        if (range_readable(save_block + kLastScoreOff, 4)) {
            out.pw_last_score =
                *reinterpret_cast<volatile const uint32_t*>(save_block + kLastScoreOff);
        }
        if (range_readable(save_block + kFultonOff, 4)) {
            out.pw_fulton =
                *reinterpret_cast<volatile const int32_t*>(save_block + kFultonOff);
        }
        // Per-mission rank array (u16 by mission id; 0 = S, 0xFFFF = never
        // cleared). Ids past the live list read as zeros, so stop at the
        // length that matches the confirmed clear/S counts.
        constexpr size_t kRankArrayOff = 0x32B4;
        constexpr size_t kRankArrayLen = 272;
        constexpr size_t kBestTimeOff = 0x29B4;
        if (range_readable(save_block + kRankArrayOff, kRankArrayLen * 2)) {
            const auto* ranks =
                reinterpret_cast<volatile const uint16_t*>(save_block + kRankArrayOff);
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
        }
        // Current mission: the id indexes both per-mission arrays, so the
        // overlay can show this mission's stored rank and best time.
        if (g_mission_id && range_readable(g_mission_id, 4)) {
            const int id = *reinterpret_cast<volatile const int32_t*>(g_mission_id);
            out.pw_mission_id = id;
            if (id > 0 && static_cast<size_t>(id) < kRankArrayLen
                && range_readable(save_block + kRankArrayOff + id * 2, 2)
                && range_readable(save_block + kBestTimeOff + id * 4, 4)) {
                const uint16_t rank =
                    *reinterpret_cast<volatile const uint16_t*>(save_block + kRankArrayOff + id * 2);
                const uint32_t best =
                    *reinterpret_cast<volatile const uint32_t*>(save_block + kBestTimeOff + id * 4);
                out.pw_cur_rank = rank == 0xFFFF ? -1 : static_cast<int>(rank);
                out.pw_cur_best = best == 0xFFFFFFFFu ? 0 : best;
            } else {
                out.pw_cur_rank = -1;
                out.pw_cur_best = 0;
            }
        }
        read_stat_families(save_block, out);
        static uintptr_t last_dump_block = 0;
        if (save_block != last_dump_block
            && range_readable(save_block + 0x40, 0x60)) {
            last_dump_block = save_block;
            LOG_INFO("MGSPW save header block %p:", reinterpret_cast<const void*>(save_block));
            log_hex_dump(reinterpret_cast<const uint8_t*>(save_block + 0x40), 0x60);
        }
        for (int i = 0; i < 16; ++i) {
            const uintptr_t rec = save_block + kWeaponArrayOff + i * kWeaponStride;
            if (!range_readable(rec, kWeaponStride)) {
                out.pw_weapon_use[i] = -1;
                continue;
            }
            const uint16_t use =
                *reinterpret_cast<volatile const uint16_t*>(rec + kWeaponUseOff);
            out.pw_weapon_use[i] = static_cast<int>(use);
        }
    }

    if (g_chararray_ptr && range_readable(g_chararray_ptr, sizeof(uintptr_t))) {
        const uintptr_t arr =
            *reinterpret_cast<volatile const uintptr_t*>(g_chararray_ptr);
        if (arr && range_readable(arr, kCharCount * sizeof(uintptr_t))) {
            const uintptr_t player =
                *reinterpret_cast<volatile const uintptr_t*>(arr);
            if (player) {
                out.pw_chararray_ok = true;
                if (range_readable(player + kHpOff, 2)) {
                    out.pw_player_hp = static_cast<int>(
                        *reinterpret_cast<volatile const int16_t*>(player + kHpOff));
                    out.current_health = out.pw_player_hp;
                }
                if (range_readable(player + kWeaponIdOff, 2)) {
                    out.pw_weapon_id = static_cast<int>(
                        *reinterpret_cast<volatile const int16_t*>(player + kWeaponIdOff));
                }
                any = true;
            }
        }
    }

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
    std::memcpy(out.seg_stage, out.pw_stage, sizeof(out.seg_stage));
    out.seg_time_seconds = out.pw_mission_ok
        ? out.play_time_seconds - seg_base.play_time_seconds
        : 0.0;
    out.seg_headshots = seg_delta(out.pw_headshots, seg_base.pw_headshots);
    out.seg_kills = seg_delta(out.pw_kills, seg_base.pw_kills);
    out.seg_tranq = seg_delta(out.pw_tranq, seg_base.pw_tranq);
    out.seg_fulton = seg_delta(out.pw_fulton_recoveries, seg_base.pw_fulton_recoveries);
    out.seg_heroism = out.pw_heroism - seg_base.pw_heroism;

    return any;
}

} // namespace bb::mgspw
