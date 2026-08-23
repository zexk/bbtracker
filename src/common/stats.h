#pragma once

#include <cstdint>

namespace bb {

enum class Difficulty : uint8_t {
    VeryEasy,
    Easy,
    Normal,
    Hard,
    Extreme,

    Count,
};

struct GameStats {
    Difficulty difficulty = Difficulty::VeryEasy;
    uint8_t difficulty_raw = 0;

    int kills = 0;
    int alerts = 0;
    int continues = 0;
    int saves = 0;
    int rations_used = 0;
    int shots_fired = 0;

    float damage_taken_bars = 0.0f;

    double play_time_seconds = 0.0;

    bool special_item_used = false;
    bool radar_off = false;

    int life_med_used = 0;
    int meals_eaten = 0;
    int severe_injuries = 0;
    int plants_captured = 0;

    bool kerotan_all_shot = false;
    bool tsuchinoko_carried = false;
    bool leech_carried = false;
};

} // namespace bb
