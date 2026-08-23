#pragma once

#include <windows.h>

namespace bb {

struct Config {
    UINT toggle_key = VK_INSERT;
    bool visible_default = true;
    float scale = 1.0f;
};

const Config& config();

bool load_config(const char* ini_path, Config& out);

} // namespace bb
