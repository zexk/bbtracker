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

};

} // namespace bb
