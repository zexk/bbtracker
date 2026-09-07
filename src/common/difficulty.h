#pragma once

#include <cstddef>

#include "stats.h"

namespace bb {

// Maps a game-specific difficulty byte to Difficulty through a lookup
// table; unknown codes fall back. MGS2 and MGS3 share the Master
// Collection table, while MGS4 (20/30/35/40) and MGS1 (-1/0/1/2) keep
// their own mappings passed to the same matcher.
struct DifficultyCode {
    int code;
    Difficulty difficulty;
};

template <size_t N>
constexpr Difficulty difficulty_from_table(int code, const DifficultyCode (&table)[N],
                                           Difficulty fallback)
{
    for (const DifficultyCode& row : table) {
        if (row.code == code) {
            return row.difficulty;
        }
    }
    return fallback;
}

inline constexpr DifficultyCode kMcDifficultyCodes[] = {
    {10, Difficulty::VeryEasy},
    {20, Difficulty::Easy},
    {30, Difficulty::Normal},
    {40, Difficulty::Hard},
    {60, Difficulty::EuroExtreme},
};

inline Difficulty master_collection_difficulty(int code)
{
    return difficulty_from_table(code, kMcDifficultyCodes, Difficulty::Extreme);
}

} // namespace bb
