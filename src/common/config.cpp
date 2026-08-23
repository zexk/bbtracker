#include "config.h"

#include <inipp.h>

#include <cstring>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace bb {
namespace {

struct KeyName {
    const char* name;
    UINT vk;
};

const KeyName kNamedKeys[] = {
    { "INSERT", VK_INSERT },   { "DELETE", VK_DELETE },     { "BACKSPACE", VK_BACK },
    { "TAB", VK_TAB },         { "HOME", VK_HOME },         { "END", VK_END },
    { "PGUP", VK_PRIOR },      { "PGDN", VK_NEXT },         { "LEFT", VK_LEFT },
    { "RIGHT", VK_RIGHT },     { "UP", VK_UP },             { "DOWN", VK_DOWN },
    { "F1", VK_F1 },           { "F2", VK_F2 },             { "F3", VK_F3 },
    { "F4", VK_F4 },           { "F5", VK_F5 },             { "F6", VK_F6 },
    { "F7", VK_F7 },           { "F8", VK_F8 },             { "F9", VK_F9 },
    { "F10", VK_F10 },         { "F11", VK_F11 },           { "F12", VK_F12 },
};

bool parse_key(const std::string& name, UINT& out)
{
    if (name.empty()) {
        return false;
    }
    for (const KeyName& k : kNamedKeys) {
        if (_stricmp(name.c_str(), k.name) == 0) {
            out = k.vk;
            return true;
        }
    }
    if (name.size() == 1) {
        char c = name[0];
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')) {
            out = static_cast<UINT>(c);
            return true;
        }
    }
    return false;
}

Config g_config{};

} // namespace

const Config& config()
{
    return g_config;
}

bool load_config(const char* ini_path, Config& out)
{
    g_config = Config{};
    out = g_config;

    inipp::Ini<char> ini;
    std::ifstream stream(ini_path);
    if (!stream.is_open()) {
        return false;
    }

    std::string key;
    if (inipp::get_value(ini.sections["overlay"], "toggle_key", key)) {
        UINT vk = 0;
        if (parse_key(key, vk)) {
            out.toggle_key = vk;
        }
    }
    int visible = -1;
    if (inipp::get_value(ini.sections["overlay"], "visible_on_start", visible) && visible >= 0
        && visible <= 1) {
        out.visible_default = visible != 0;
    }
    float scale = 0.0f;
    if (inipp::get_value(ini.sections["overlay"], "scale", scale) && scale >= 0.25f && scale <= 8.0f) {
        out.scale = scale;
    }

    std::string diff;
    if (inipp::get_value(ini.sections["stats"], "difficulty", diff)) {
        static const std::pair<const char*, int> kDiffMap[] = {
            {"veryeasy", 0}, {"very easy", 0}, {"ve", 0},
            {"easy", 1},     {"e", 1},
            {"normal", 2},   {"n", 2},
            {"hard", 3},     {"h", 3},
            {"extreme", 4},  {"x", 4}, {"euro", 4}, {"ee", 4},
        };
        for (const auto& [name, val] : kDiffMap) {
            if (_stricmp(diff.c_str(), name) == 0) {
                out.difficulty_override = val;
                break;
            }
        }
    }

    int hunt = -1;
    if (inipp::get_value(ini.sections["stats"], "hunt_value", hunt) && hunt >= 0
        && hunt <= 0xFFFF) {
        out.hunt_value = hunt;
    }

    int radar_off = -1;
    if (inipp::get_value(ini.sections["stats"], "radar_off", radar_off) && radar_off >= 0
        && radar_off <= 1) {
        out.radar_off_override = radar_off;
    }

    g_config = out;
    return true;
}

} // namespace bb
