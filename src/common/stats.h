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

    int damage_taken_units = 0;

    double play_time_seconds = 0.0;

    bool special_item_used = false;
    uint8_t special_items_mask = 0;
    bool radar_off = false;
    uint8_t radar_type = 0;

    int life_med_used = 0;
    int meals_eaten = 0;
    int severe_injuries = 0;
    int plants_captured = 0;

    bool kerotan_all_shot = false;
    uint64_t kerotan_mask = 0; // bit i = kerotan i shot
    uint8_t kerotan_count = 0;
    bool tsuchinoko_carried = false;
    bool leech_carried = false;

    uint8_t difficulty_game_byte = 0;

    int headshots = 0;
    int cqc_chokes = 0;
    int hold_ups = 0;
    int body_searches = 0;
    int items_given = 0;
    int militia_praise = 0;
    int weapons_owned = 0;
    int combat_highs = 0;
    int side_rolls = 0;
    int forward_rolls = 0;
    int crawl_seconds = 0;
    int crouch_seconds = 0;
    int box_seconds = 0;
    int wall_seconds = 0;
    int playboy_pages = 0;
    int scan_plug_uses = 0;
    int knife_kills = 0;
    int weapons_picked_up = 0;
};

} // namespace bb
