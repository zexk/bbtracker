constexpr IdColors kMgs1Colors{{0.42f, 0.88f, 0.66f, 1}, {0.88f, 0.72f, 0.28f, 1}, {0.90f, 0.32f, 0.30f, 1}};

void apply_theme_mgs1(ImGuiStyle& style)
{
    ImVec4* colors = style.Colors;
    const ImVec4 text{0.55f, 0.76f, 0.69f, 1.0f};
    const ImVec4 dim{0.32f, 0.47f, 0.43f, 1.0f};
    const ImVec4 panel{0.03f, 0.07f, 0.07f, 1.0f};
    const ImVec4 border{0.43f, 0.72f, 0.64f, 1.0f};
    const ImVec4 frame{0.08f, 0.17f, 0.16f, 1.0f};
    const ImVec4 hover{0.20f, 0.43f, 0.38f, 1.0f};
    const ImVec4 active{0.30f, 0.58f, 0.51f, 1.0f};
    const ImVec4 title{0.05f, 0.13f, 0.13f, 1.0f};
    const ImVec4 title_active{0.12f, 0.27f, 0.25f, 1.0f};
    const ImVec4 accent{0.60f, 0.94f, 0.82f, 1.0f};
    const ImVec4 idle{0.10f, 0.23f, 0.21f, 1.0f};
    const ImVec4 row{0.08f, 0.18f, 0.17f, 1.0f};

    colors[ImGuiCol_Text]              = text;
    colors[ImGuiCol_TextDisabled]      = dim;
    colors[ImGuiCol_WindowBg]          = with_alpha(panel, panel_alpha());
    colors[ImGuiCol_Border]            = with_alpha(border, 0.65f);
    colors[ImGuiCol_FrameBg]           = frame;
    colors[ImGuiCol_FrameBgHovered]    = hover;
    colors[ImGuiCol_FrameBgActive]     = active;
    colors[ImGuiCol_TitleBg]           = title;
    colors[ImGuiCol_TitleBgActive]     = title_active;
    colors[ImGuiCol_CheckMark]         = accent;
    colors[ImGuiCol_SliderGrab]        = border;
    colors[ImGuiCol_SliderGrabActive]  = accent;
    colors[ImGuiCol_Button]            = idle;
    colors[ImGuiCol_ButtonHovered]     = hover;
    colors[ImGuiCol_ButtonActive]      = active;
    colors[ImGuiCol_Header]            = idle;
    colors[ImGuiCol_HeaderHovered]     = hover;
    colors[ImGuiCol_HeaderActive]      = active;
    colors[ImGuiCol_Separator]         = with_alpha(border, 0.65f);
    colors[ImGuiCol_TableRowBgAlt]     = with_alpha(row, 0.55f);
    colors[ImGuiCol_ResizeGrip]        = with_alpha(border, 0.30f);
    colors[ImGuiCol_ResizeGripHovered] = with_alpha(accent, 0.70f);
    colors[ImGuiCol_ResizeGripActive]  = accent;
}

const char* mgs1_area_name(const char* stage)
{
    static constexpr AreaName kAreas[] = {
        {"03a", "Cell"},           {"03b", "Medi Room"},
        {"03c", "Medi Room"},      {"03d", "Cell"},
        {"04a", "Armory"},         {"04b", "Armory South"},
        {"04c", "Armory South"},   {"07a", "Nuke Building B1"},
        {"07b", "Commander's Room"}, {"07c", "Nuke Building B1"},
        {"08a", "Nuke Building B2"}, {"08b", "Lab"},
        {"08c", "Lab Hallway"},    {"11a", "Comms Tower A"},
        {"11b", "Comms Tower A Roof"}, {"11c", "Comms Tower B"},
        {"11d", "Comms Tower A Wall"}, {"11e", "Comms Tower B Elevator"},
        {"11g", "Comms Tower A Roof"}, {"11h", "Comms Tower B Roof"},
        {"11i", "Walkway"},        {"15a", "Warehouse"},
        {"15b", "Warehouse North"}, {"15c", "Warehouse"},
        {"16a", "Underground Base 1"}, {"16b", "Underground Base 2"},
        {"16c", "Underground Base 3"}, {"16d", "Command Room"},
        {"16e", "Underground Base 3"}, {"19a", "Escape Route 1"},
        {"19b", "Escape Route 2"},
        {"00", "Dock"},            {"01", "Heliport"},
        {"02", "Tank Hangar"},     {"05", "Canyon"},
        {"06", "Nuke Building 1F"}, {"09", "Cave"},
        {"10", "Underground Passage"}, {"12", "Snowfield"},
        {"13", "Blast Furnace"},   {"14", "Cargo Elevator"},
        {"17", "Supply Route"},    {"18", "Supply Route"},
    };
    if (!stage || (stage[0] != 's' && stage[0] != 'd')) {
        return nullptr;
    }
    const char* code = stage + 1;
    for (const AreaName& area : kAreas) {
        const size_t len = std::strlen(area.code);
        if (std::strncmp(code, area.code, len) == 0
            && (len == 2 || code[len] == '\0')) {
            return area.name;
        }
    }
    return nullptr;
}

DifficultyDisplay difficulty_mgs1(const GameStats& stats)
{
    return {difficulty_name(stats.difficulty), true,
            stats.difficulty == Difficulty::Extreme};
}

bool requirement_mgs1(const codename::ReqStatus& req, const GameStats& stats,
                      char* out, size_t size)
{
    if (std::strcmp(req.label, "radar") != 0) return false;
    snprintf(out, size, "%s", stats.radar_off ? "OFF" : "ON");
    return true;
}

void summary_extra_mgs1(const GameStats& stats)
{
    summary_count("saves", stats.saves);
    summary_pair("health", stats.current_health, stats.max_health);
    if (stats.diazepam_frames > 0) {
        // Diazepam timer ticks at 30fps while game time runs at 60.
        char buf[24];
        snprintf(buf, sizeof(buf), "%.1fs", stats.diazepam_frames / 30.0);
        dim_row("diazepam", buf);
    }
}
