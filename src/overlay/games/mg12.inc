constexpr IdColors kMg1Colors{{0.48f, 0.72f, 0.38f, 1}, {0.80f, 0.48f, 0.04f, 1}, {0.82f, 0.20f, 0.10f, 1}};
constexpr IdColors kMg2Colors{{0.38f, 0.72f, 0.52f, 1}, {0.82f, 0.66f, 0.20f, 1}, {0.85f, 0.28f, 0.28f, 1}};

void apply_theme_mg1(ImGuiStyle& style)
{
    ImVec4* colors = style.Colors;
    const ImVec4 text{0.72f, 0.73f, 0.72f, 1.0f};
    const ImVec4 dim{0.43f, 0.44f, 0.43f, 1.0f};
    const ImVec4 panel{0.01f, 0.01f, 0.01f, 1.0f};
    const ImVec4 border{0.58f, 0.59f, 0.57f, 1.0f};
    const ImVec4 frame{0.16f, 0.16f, 0.15f, 1.0f};
    const ImVec4 hover{0.40f, 0.18f, 0.04f, 1.0f};
    const ImVec4 active{0.62f, 0.12f, 0.06f, 1.0f};
    const ImVec4 title{0.22f, 0.03f, 0.02f, 1.0f};
    const ImVec4 title_active{0.55f, 0.07f, 0.04f, 1.0f};
    const ImVec4 accent{0.80f, 0.48f, 0.04f, 1.0f};
    const ImVec4 idle{0.20f, 0.20f, 0.19f, 1.0f};

    colors[ImGuiCol_Text]              = text;
    colors[ImGuiCol_TextDisabled]      = dim;
    colors[ImGuiCol_WindowBg]          = with_alpha(panel, panel_alpha());
    colors[ImGuiCol_Border]            = with_alpha(border, 0.72f);
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
    colors[ImGuiCol_Separator]         = with_alpha(border, 0.68f);
    colors[ImGuiCol_TableRowBgAlt]     = with_alpha(frame, 0.70f);
    colors[ImGuiCol_ResizeGrip]        = with_alpha(active, 0.35f);
    colors[ImGuiCol_ResizeGripHovered] = with_alpha(accent, 0.75f);
    colors[ImGuiCol_ResizeGripActive]  = accent;
}

void apply_theme_mg2(ImGuiStyle& style)
{
    ImVec4* colors = style.Colors;
    const ImVec4 text{0.72f, 0.75f, 0.76f, 1.0f};
    const ImVec4 dim{0.42f, 0.46f, 0.50f, 1.0f};
    const ImVec4 panel{0.00f, 0.01f, 0.03f, 1.0f};
    const ImVec4 border{0.12f, 0.30f, 0.78f, 1.0f};
    const ImVec4 frame{0.03f, 0.07f, 0.17f, 1.0f};
    const ImVec4 hover{0.08f, 0.20f, 0.48f, 1.0f};
    const ImVec4 active{0.12f, 0.30f, 0.70f, 1.0f};
    const ImVec4 title{0.02f, 0.05f, 0.16f, 1.0f};
    const ImVec4 title_active{0.04f, 0.11f, 0.34f, 1.0f};
    const ImVec4 accent{0.42f, 0.62f, 0.94f, 1.0f};
    const ImVec4 idle{0.04f, 0.10f, 0.25f, 1.0f};
    const ImVec4 slider{0.25f, 0.43f, 0.78f, 1.0f};

    colors[ImGuiCol_Text]              = text;
    colors[ImGuiCol_TextDisabled]      = dim;
    colors[ImGuiCol_WindowBg]          = with_alpha(panel, panel_alpha());
    colors[ImGuiCol_Border]            = with_alpha(border, 0.80f);
    colors[ImGuiCol_FrameBg]           = frame;
    colors[ImGuiCol_FrameBgHovered]    = hover;
    colors[ImGuiCol_FrameBgActive]     = active;
    colors[ImGuiCol_TitleBg]           = title;
    colors[ImGuiCol_TitleBgActive]     = title_active;
    colors[ImGuiCol_CheckMark]            = accent;
    colors[ImGuiCol_SliderGrab]           = slider;
    colors[ImGuiCol_SliderGrabActive]     = accent;
    colors[ImGuiCol_Button]               = idle;
    colors[ImGuiCol_ButtonHovered]        = hover;
    colors[ImGuiCol_ButtonActive]         = active;
    colors[ImGuiCol_Header]               = idle;
    colors[ImGuiCol_HeaderHovered]        = hover;
    colors[ImGuiCol_HeaderActive]         = active;
    colors[ImGuiCol_Separator]            = with_alpha(border, 0.75f);
    colors[ImGuiCol_TableRowBgAlt]        = with_alpha(frame, 0.72f);
    colors[ImGuiCol_ResizeGrip]           = with_alpha(border, 0.35f);
    colors[ImGuiCol_ResizeGripHovered] = with_alpha(accent, 0.75f);
    colors[ImGuiCol_ResizeGripActive]  = accent;
}

DifficultyDisplay difficulty_mg12(const GameStats& stats)
{
    const bool original = stats.difficulty == Difficulty::Extreme;
    return {original ? "Original" : "Easy", true,
            original || stats.difficulty == Difficulty::Easy};
}

bool requirement_mg12(const codename::ReqStatus& req, const GameStats& stats,
                      char* out, size_t size)
{
    if (std::strcmp(req.label, "special items") != 0) return false;
    snprintf(out, size, "%s", stats.special_item_used ? "Infinity Bandana" : "NONE");
    return true;
}
