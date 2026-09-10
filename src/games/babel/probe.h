#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../../common/stats.h"

namespace bb::babel {

constexpr size_t kWramBankSize = 0x1000;
constexpr size_t kWramSize = 8 * kWramBankSize;

constexpr bool all_zero(const uint8_t* data, size_t offset, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        if (data[offset + i] != 0) return false;
    }
    return true;
}

constexpr bool run_reset(const uint8_t* wram)
{
    return all_zero(wram, 0x4EE, 14)
        && all_zero(wram + 6 * kWramBankSize, 0xF4A, 14);
}

template <typename T>
T read_wram(const uint8_t* data, size_t offset)
{
    T value{};
    std::memcpy(&value, data + offset, sizeof(value));
    return value;
}

inline bool decode_wram(const uint8_t* data, GameStats& out)
{
    const uint8_t* const bank6 = data + 6 * kWramBankSize;
    const uint8_t difficulty = data[0x4E7];
    const uint8_t stage = data[0x46C];
    const uint8_t mode = data[0x0F3];
    if (difficulty > 3 || stage > 12 || (mode & 0x0F) != 0) return false;

    const uint8_t frames = data[0x4F8];
    const uint8_t seconds = data[0x4F9];
    const uint8_t minutes = data[0x4FA];
    const uint8_t hours = data[0x4FB];
    const uint8_t career_seconds = bank6[0xF55];
    const uint8_t career_minutes = bank6[0xF56];
    const uint8_t career_hours = bank6[0xF57];
    if (frames >= 60 || seconds >= 60 || minutes >= 60 || hours >= 100
        || career_seconds >= 60 || career_minutes >= 60 || career_hours >= 100) {
        return false;
    }

    out = {};
    out.difficulty_raw = difficulty;
    out.difficulty_game_byte = difficulty;
    out.difficulty = difficulty == 0 ? Difficulty::Easy
        : difficulty == 1 ? Difficulty::Normal
        : difficulty == 2 ? Difficulty::Hard : Difficulty::Extreme;
    out.mission = stage;
    out.alerts = read_wram<uint16_t>(data, 0x4EE)
        + read_wram<uint16_t>(bank6, 0xF4A);
    out.kills = read_wram<uint16_t>(data, 0x4F0)
        + read_wram<uint16_t>(bank6, 0xF4C);
    out.rations_used = read_wram<uint16_t>(data, 0x4F2)
        + read_wram<uint16_t>(bank6, 0xF4E);
    out.saves = data[0x44A];
    out.play_time_seconds = career_seconds + career_minutes * 60.0
        + career_hours * 3600.0 + seconds + minutes * 60.0 + hours * 3600.0
        + frames / 60.0;
    return true;
}

bool poll_stats(GameStats& out);

}
