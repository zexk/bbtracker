#pragma once

#include <cstdint>

namespace bb {

enum class Difficulty : uint8_t {
    VeryEasy,
    Easy,
    Normal,
    Hard,
    Extreme,
    EuroExtreme,

    Count,
};

struct GameStats {
    Difficulty difficulty = Difficulty::VeryEasy;
    uint8_t difficulty_raw = 0;
    int mission = -1;
    char area_code[8] = {};

    int kills = 0;
    int alerts = 0;
    int continues = 0;
    int saves = 0;
    int rations_used = 0;
    int shots_fired = 0;
    int times_seen = 0;
    int clearing_escapes = 0;
    int mechs_destroyed = 0;
    int pull_ups = 0;

    int cqc_chokes = 0;
    int headshots = 0;
    int knife_defeats = 0;
    int cqc_holds = 0;
    int items_given = 0;
    int praises = 0;
    int weapons_acquired = 0;
    int body_searches = 0;
    int hold_ups = 0;
    int magazine_pages = 0;
    int syringe_uses = 0;
    int side_rolls = 0;
    int forward_rolls = 0;
    int pickups = 0;
    int combat_highs = 0;
    int flashbacks_viewed = 0;

    double box_time_seconds = 0.0;
    double wall_time_seconds = 0.0;
    double crawl_time_seconds = 0.0;
    double crouch_time_seconds = 0.0;

    int damage_taken_units = 0;
    double damage_taken_bars = -1.0; // exact when game exposes end-screen bars

    double play_time_seconds = 0.0;

    bool special_item_used = false;
    bool sea_louse = false;
    uint16_t special_items_mask = 0;
    bool radar_off = false;
    uint8_t radar_type = 0;
    uint8_t alert_state = 0;
    bool alert_state_available = false;

    int current_health = 0;
    int max_health = 0;
    int diazepam_frames = 0;

    int life_med_used = 0;
    int meals_eaten = 0;
    int severe_injuries = 0;
    int plants_captured = 0;
    int kerotans = 0;
    uint64_t capture_mask = 0;
    uint64_t kerotan_mask = 0;
    bool leech_attached = false;
    bool tsuchinoko_alive = false;

    uint8_t difficulty_game_byte = 0;
    bool mgs1_integral = false;
    bool mgs1_japanese_original = false;

    // Peace Walker probe (FOXHOUND tracker bootstrap, all unverified).
    uint64_t pw_mission_raw = 0;      // [PW_MISSIONTIME] ticks 300/s of active game time
    uint64_t pw_mission_aux = 0;      // [PW_MISSIONTIME+0x08] second accumulator
    uint32_t pw_area = 0;             // [PW_MISSIONTIME+0x10] area timer, mirrors stage?
    uint32_t pw_mission_secondary = 0;// [PW_MISSIONTIME+0x14] mirror of area timer
    uint32_t pw_total_play = 0;       // [SAVEROOT+0x84] total play, ticks ~1/s
    uint32_t pw_stage_play = 0;       // [SAVEROOT+0x88] stage play, ticks fast (ms?)
    char pw_stage[32] = {};           // [SAVEROOT+0x54] stage string
    int pw_player_hp = 0;             // [CHARARRAY[0]+0x11BE] u16, 8000 = full
    int pw_weapon_id = -1;            // [CHARARRAY[0]+0x14B8]
    int pw_heroism = 0;               // [SAVEROOT+0x64F4] validated lifetime Heroism
    int pw_heroism_delta = 0;         // [SAVEROOT+0x64EC] last-mission delta
    uint32_t pw_gmp = 0;              // [SAVEROOT+0xB52C] validated funds
    uint32_t pw_last_best_a = 0;      // [SAVEROOT+0x586C] last-mission best twin slot A
    uint32_t pw_last_best_b = 0;      // [SAVEROOT+0x5874] last-mission best twin slot B
    int pw_clears = -1;               // [SAVEROOT+0x656C] validated global clear count (replays count)
    // S ranks come from the per-mission array below, not from a scalar: the
    // offset once read as an S count turned out to be unrelated.
    int pw_fulton = -1;               // [SAVEROOT+0x130] contested: Fulton stock or inspected-weapon XP-to-next
    // FOXHOUND lifetime inputs, resolved by descriptor ID (table moves).
    // -1 = unresolved. Confirmed by quantified missions: headshots +1
    // on exactly-1-headshot run, kills +3 on 3-kill run, tranq +2 on
    // 2-tranq run. Max across id matches (live copy leads stale
    // snapshot copies). Fulton single-evidence (one +1).
    int pw_headshots = -1; // id 0x4420031
    int pw_kills = -1;     // ids 0x420008, 0x2007C, 0x200E0
    int pw_tranq = -1;     // ids 0x442002E, 0x200F9
    // id 0x2008E: Fulton recoveries. Confirmed over four runs, including an
    // 8-extraction main op where the in-game results screen showed 8.
    int pw_fulton_recoveries = -1;
    // id 0x2008F: prisoners extracted, counted apart from enemies. A run the
    // game summarised as "6 enemies extracted and 1 prisoner" moved 0x2008E
    // by 6 and this by 1.
    int pw_prisoner_extractions = -1;
    // Confirmed over three quantified runs (clean / 6-kill+alert /
    // 0-kill+alert): 0x442011E only moves when the run had no alert,
    // 0x442011F only when it had no kill. 0x44200DC moves on every clear.
    int pw_noitem_clears = -1;   // id 0x44200DC, "no recovery items used"
    int pw_holdups = -1;         // id 0x4420030, "Total Hold-ups"
    int pw_noalert_clears = -1;  // id 0x442011E
    int pw_nokill_clears = -1;   // id 0x442011F
    // Per-mission rank array (save+0x32B4, u16 by mission id): 0 = S,
    // 0xFFFF = never cleared. Best time lives at save+0x29B4 + 4*id.
    int pw_unique_cleared = -1;
    int pw_s_missions = -1;
    // Current mission (-1 outside a mission) plus its stored result:
    // rank -1 = never cleared, best 0 = no time on record.
    int pw_mission_id = -1;
    int pw_cur_rank = -1;
    uint32_t pw_cur_best = 0;
    // save+0x278, mirrored at +0x2A0/+0x1FBF4/+0x1FC1C. Moves per mission
    // (4758 -> 6000 -> 8000 -> 6000) but is NOT the results score: regular
    // missions display no score at all. Unidentified; shown in Forensics.
    uint32_t pw_last_score = 0;
    // Career alerts (id 0x420002) plus the game's own per-mission tally,
    // the descriptor's +0x18 field: it counts up live during a mission and
    // the game clears it at mission start, so it beats latching baselines.
    // -1 = unresolved.
    int pw_alerts = -1;
    int pw_m_kills = -1;
    int pw_m_alerts = -1;
    int pw_m_tranq = -1;
    int pw_m_headshots = -1;
    // Non-headshot kills (id 0x200ED). Kills minus this is the lethal
    // headshot count, and headshots minus that is the tranq headshot count.
    int pw_body_kills = -1;
    int pw_m_body_kills = -1;
    // Per-weapon-type takedowns. The game keeps one counter per type (11 of
    // them, CQC and stun rod included); these two are the ones identified so
    // far. pw_tranq/pw_kills are the non-lethal/lethal totals they roll into.
    // Counters are per type AND per lethality: 0x200Ex is the lethal bank,
    // 0x200Fx the non-lethal one.
    int pw_pistol_takedowns = -1;   // id 0x200F9, non-lethal
    int pw_ar_takedowns = -1;       // id 0x200E0, lethal
    int pw_shotgun_takedowns = -1;  // id 0x200E4, lethal
    int pw_sniper_takedowns = -1;   // id 0x200E1, lethal
    int pw_lmg_takedowns = -1;      // id 0x200E2, lethal
    int pw_sniper_nonlethal = -1;   // id 0x200FB, non-lethal (Mosin)
    int pw_pistol_lethal = -1;      // id 0x200DF, lethal
    int pw_cqc_takedowns = -1;      // id 0x20104, chokes and slams alike
    int pw_grenade_takedowns = -1;  // id 0x200E6, lethal
    int pw_rocket_takedowns = -1;   // id 0x200E5, lethal
    int pw_placed_takedowns = -1;   // id 0x200E8, lethal (C4 and the like)
    int pw_stealth_kills = -1;      // id 0x2007C, kills on unaware enemies
    int pw_damage_taken = -1;       // id 0x20023, career damage taken (8000 = a full bar)
    // Live per-sortie deltas, differenced client-side at stage change
    // (action careers tick live mid-mission; heroism/XP/GMP settle at
    // results, so their segments only move post-results).
    char seg_stage[32] = {};
    double seg_time_seconds = 0.0;
    int seg_headshots = 0;
    int seg_kills = 0;
    int seg_tranq = 0;
    int seg_fulton = 0;
    int seg_heroism = 0;
    bool pw_saveroot_ok = false;
    bool pw_mission_ok = false;
    bool pw_chararray_ok = false;
    // First 16 weapon-record XP values ([rec+0x14] u16, stride 0x1C;
    // per-level pool, resets on level-up). -1 = unreadable.
    int pw_weapon_use[16] = {-1, -1, -1, -1, -1, -1, -1, -1,
                             -1, -1, -1, -1, -1, -1, -1, -1};

};

} // namespace bb
