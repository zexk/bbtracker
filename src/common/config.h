#pragma once

#include <windows.h>

namespace bb {

struct Config {
    UINT toggle_key = VK_F3;
    bool visible_default = true;
    float scale = 1.0f;
    int difficulty_override = -1; // -1 = auto; else Difficulty enum value
};

const Config& config();

bool load_config(const char* ini_path, Config& out);

} // namespace bb
