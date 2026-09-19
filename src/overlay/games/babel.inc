constexpr IdColors kBabelColors{{0.00f, 0.63f, 0.36f, 1}, {0.94f, 0.78f, 0.00f, 1}, {0.97f, 0.00f, 0.00f, 1}};

// Ghost Babel's HUD is a Game Boy Color palette: pure black plates, a
// one-pixel amber frame with squared corners, white pixel text, the item
// slots in pure red, and the LIFE bar in teal. Nothing here is tinted or
// blended — the console had no alpha, so the plates stay flat and only the
// window itself is translucent enough to play under.
void apply_theme_babel(ImGuiStyle& style)
{
    ImVec4* colors = style.Colors;
    // Ghost Babel's title menu: black ground, white text, the live entry in
    // the menu green, the second entry in pure red, and unavailable entries
    // in flat gray. There is no yellow on that screen, so the middle band is
    // inferred from the same flat, fully saturated GBC palette.
    // Only the chrome this panel draws: no tabs, no scrollbar or resize grip
    // under AlwaysAutoResize, no frames, headers or selectables. The button
    // colors are here for the title bar's close box.
    const ImVec4 text{0.97f, 0.97f, 0.97f, 1.0f};
    const ImVec4 dim{0.56f, 0.56f, 0.56f, 1.0f};
    const ImVec4 panel{0.00f, 0.00f, 0.00f, 1.0f};
    const ImVec4 rule{0.00f, 0.63f, 0.36f, 1.0f};
    const ImVec4 title{0.00f, 0.00f, 0.00f, 1.0f};
    const ImVec4 title_active{0.00f, 0.20f, 0.11f, 1.0f};
    const ImVec4 button{0.00f, 0.16f, 0.09f, 1.0f};
    const ImVec4 button_hover{0.00f, 0.42f, 0.24f, 1.0f};
    const ImVec4 row{0.06f, 0.06f, 0.06f, 1.0f};

    colors[ImGuiCol_Text]              = text;
    colors[ImGuiCol_TextDisabled]      = dim;
    colors[ImGuiCol_WindowBg]          = with_alpha(panel, panel_alpha());
    colors[ImGuiCol_Border]            = rule;
    colors[ImGuiCol_TitleBg]           = title;
    colors[ImGuiCol_TitleBgActive]     = title_active;
    colors[ImGuiCol_Button]            = button;
    colors[ImGuiCol_ButtonHovered]     = button_hover;
    colors[ImGuiCol_ButtonActive]      = rule;
    colors[ImGuiCol_Separator]         = with_alpha(rule, 0.55f);
    colors[ImGuiCol_TableBorderLight]  = with_alpha(dim, 0.35f);
    colors[ImGuiCol_TableRowBgAlt]     = with_alpha(row, 0.55f);
}

DifficultyDisplay difficulty_babel(const GameStats& stats)
{
    constexpr const char* names[]{"Easy", "Normal", "Hard", "Very Hard"};
    return {stats.difficulty_raw < std::size(names) ? names[stats.difficulty_raw]
                                      : difficulty_name(stats.difficulty),
            true, stats.difficulty == Difficulty::Extreme};
}

// Babel's elite rank name changes by difficulty; rank kind decides color.
ImVec4 match_color_babel(const codename::Match& match, const IdColors& colors)
{
    if (match.kind == codename::Kind::Elite) return colors.green;
    if (match.kind == codename::Kind::Worst) return colors.red;
    return ImGui::GetStyleColorVec4(ImGuiCol_Text);
}
