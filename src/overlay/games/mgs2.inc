constexpr IdColors kMgs2Colors{{0.42f, 0.82f, 0.52f, 1}, {0.92f, 0.70f, 0.24f, 1}, {0.76f, 0.19f, 0.11f, 1}};
constexpr const char* kMgs2SpecialItems[] = {
    "Infinity Bandana", "Infinity Wig", "O2 Wig", "Grip Wig", "Stealth Camo"};
constexpr uint16_t kLastKnownStoryStep = 600;
constexpr uint8_t kRadarTypeB = 0x20;

void apply_theme_mgs2(ImGuiStyle& style)
{
    ImVec4* colors = style.Colors;
    style.ScrollbarRounding = 0.0f;
    const ImVec4 text{0.61f, 0.69f, 0.64f, 1.0f};
    const ImVec4 dim{0.34f, 0.42f, 0.38f, 1.0f};
    const ImVec4 panel{0.02f, 0.05f, 0.04f, 1.0f};
    const ImVec4 border{0.38f, 0.53f, 0.47f, 1.0f};
    const ImVec4 frame{0.07f, 0.13f, 0.11f, 1.0f};
    const ImVec4 hover{0.28f, 0.10f, 0.07f, 1.0f};
    const ImVec4 active{0.48f, 0.13f, 0.08f, 1.0f};
    const ImVec4 title{0.04f, 0.09f, 0.07f, 1.0f};
    const ImVec4 title_active{0.20f, 0.08f, 0.06f, 1.0f};
    const ImVec4 accent{0.76f, 0.19f, 0.11f, 1.0f};
    const ImVec4 idle{0.10f, 0.18f, 0.15f, 1.0f};
    const ImVec4 row{0.08f, 0.15f, 0.12f, 1.0f};
    const ImVec4 slider{0.48f, 0.61f, 0.55f, 1.0f};
    const ImVec4 scrollbar{0.23f, 0.34f, 0.29f, 1.0f};

    colors[ImGuiCol_Text]                 = text;
    colors[ImGuiCol_TextDisabled]         = dim;
    colors[ImGuiCol_WindowBg]             = with_alpha(panel, panel_alpha());
    colors[ImGuiCol_Border]               = with_alpha(border, 0.70f);
    colors[ImGuiCol_FrameBg]              = frame;
    colors[ImGuiCol_FrameBgHovered]       = hover;
    colors[ImGuiCol_FrameBgActive]        = active;
    colors[ImGuiCol_TitleBg]              = title;
    colors[ImGuiCol_TitleBgActive]        = title_active;
    colors[ImGuiCol_CheckMark]            = accent;
    colors[ImGuiCol_SliderGrab]           = slider;
    colors[ImGuiCol_SliderGrabActive]     = accent;
    colors[ImGuiCol_Button]               = idle;
    colors[ImGuiCol_ButtonHovered]        = hover;
    colors[ImGuiCol_ButtonActive]         = active;
    colors[ImGuiCol_Header]               = idle;
    colors[ImGuiCol_HeaderHovered]        = hover;
    colors[ImGuiCol_HeaderActive]         = active;
    colors[ImGuiCol_Tab]                  = frame;
    colors[ImGuiCol_TabHovered]           = hover;
    colors[ImGuiCol_TabActive]            = active;
    colors[ImGuiCol_TabUnfocused]         = title;
    colors[ImGuiCol_TabUnfocusedActive]   = title_active;
    colors[ImGuiCol_Separator]            = with_alpha(border, 0.70f);
    colors[ImGuiCol_TableRowBgAlt]        = with_alpha(row, 0.55f);
    colors[ImGuiCol_ResizeGrip]           = with_alpha(slider, 0.30f);
    colors[ImGuiCol_ResizeGripHovered]    = with_alpha(accent, 0.70f);
    colors[ImGuiCol_ResizeGripActive]     = accent;
    colors[ImGuiCol_ScrollbarBg]          = with_alpha(title, 0.70f);
    colors[ImGuiCol_ScrollbarGrab]        = with_alpha(scrollbar, 0.85f);
    colors[ImGuiCol_ScrollbarGrabHovered] = with_alpha(active, 0.90f);
    colors[ImGuiCol_ScrollbarGrabActive]  = with_alpha(accent, 0.95f);
}

const char* mgs2_area_name(const char* code)
{
    static constexpr AreaName kAreas[] = {
        {"w00a", "Aft Deck"}, {"w00b", "Aft Deck (Olga)"},
        {"w00c", "Navigational Deck"}, {"w01a", "Deck-A, Crew's Quarters"},
        {"w01b", "Deck-A, Crew's Quarters, Starboard"},
        {"w01c", "Deck-C, Crew's Quarters"}, {"w01d", "Deck-D, Crew's Quarters"},
        {"w01e", "Deck-E, Bridge"}, {"w01f", "Deck-A, Crew's Lounge"},
        {"w02a", "Engine Room"}, {"w03a", "Deck 2, Port"},
        {"w03b", "Deck 2, Starboard"}, {"w04a", "Hold No. 1"},
        {"w04b", "Hold No. 2"}, {"w04c", "Hold No. 3"},
        {"w11a", "Strut A, Deep Sea Dock"}, {"w11b", "Strut A, Deep Sea Dock"},
        {"w11c", "Strut A, Deep Sea Dock"}, {"w12a", "Strut A, Roof"},
        {"w12b", "Strut A, Pump Room"}, {"w12c", "Strut A, Roof"},
        {"w13a", "AB Connecting Bridge"}, {"w13b", "AB Connecting Bridge"},
        {"w14a", "Strut B, Transformer Room"}, {"w15a", "BC Connecting Bridge"},
        {"w15b", "BC Connecting Bridge"}, {"w16a", "Strut C, Dining Hall"},
        {"w16b", "Strut C, Dining Hall"}, {"w17a", "CD Connecting Bridge"},
        {"w18a", "Strut D, Sediment Pool"}, {"w19a", "DE Connecting Bridge"},
        {"w20a", "Strut E, Parcel Room"}, {"w20b", "Strut E, Heliport"},
        {"w20c", "Strut E, Heliport"}, {"w20d", "Strut E, Heliport"},
        {"w21a", "EF Connecting Bridge"}, {"w21b", "EF Connecting Bridge"},
        {"w22a", "Strut F, Warehouse"}, {"w23a", "FA Connecting Bridge"},
        {"w23b", "FA Connecting Bridge"}, {"w24a", "Shell 1 Core, 1F"},
        {"w24b", "Shell 1 Core, B1"}, {"w24c", "Shell 1 Core, B1 Hall"},
        {"w24d", "Shell 1 Core, B2"}, {"w24e", "Shell 1 Core"},
        {"w25a", "Shell 1-2 Connecting Bridge"},
        {"w25b", "Shell 1-2 Connecting Bridge"}, {"w25c", "Strut L Perimeter"},
        {"w25d", "KL Connecting Bridge"}, {"w28a", "Sewage Treatment Facility"},
        {"w31a", "Shell 2 Core, 1F"}, {"w31b", "Shell 2 Core, B1"},
        {"w31c", "Shell 2 Core, B1 Filtration Chamber"},
        {"w31d", "Shell 2 Core, 1F"}, {"w31f", "Shell 2 Core"},
        {"w32a", "Oil Fence"}, {"w32b", "Oil Fence"},
        {"w41a", "Arsenal Gear, Stomach"}, {"w42a", "Arsenal Gear, Jejunum"},
        {"w43a", "Arsenal Gear, Ascending Colon"}, {"w44a", "Arsenal Gear, Ileum"},
        {"w45a", "Arsenal Gear, Sigmoid Colon"}, {"w46a", "Arsenal Gear, Rectum"},
        {"w51a", "Arsenal Gear"}, {"w61a", "Federal Hall"},
    };
    return exact_area_name(code, kAreas);
}

const char* mgs2_area_state(const GameStats& stats)
{
    if (stats.mgs2_p_story == kUnknownMgs2Story
        || stats.mgs2_p_story > kLastKnownStoryStep) return nullptr;
    for (const mgs2::DogTagAreaState& state : mgs2::kDogTagAreaStates) {
        if (std::strcmp(stats.area_code, state.area) == 0) {
            return stats.mgs2_p_story >= state.story ? state.after : state.before;
        }
    }
    struct FlagState {
        const char* area;
        int flag;
        const char* before;
        const char* after;
    };
    static constexpr FlagState kFlagStates[] = {
        {"w02a", 1, "before door repair", "after door repair"},
        {"w01b", 3, "before shadow", "after shadow"},
        {"w00c", 4, "before right door", "after right door"},
        {"w45a", 6, "before Tengu", "after Tengu"},
    };
    for (const FlagState& state : kFlagStates) {
        if (std::strcmp(stats.area_code, state.area) == 0) {
            const bool set = (stats.mgs2_dog_tag_flags & (uint32_t{1} << state.flag)) != 0;
            return set ? state.after : state.before;
        }
    }
    return nullptr;
}

void draw_mgs2_dog_tags(const GameStats& stats, int scroll)
{
    const uint8_t difficulty = static_cast<uint8_t>(stats.difficulty);
    int available = 0;
    int collected = 0;
    for (const auto& tag : mgs2::kDogTags) {
        if (!mgs2::dog_tag_available(tag, stats.mission, difficulty)) continue;
        ++available;
        collected += mgs2::dog_tag_collected(stats.dog_tag_mask, tag.id);
    }

    ImGui::Text("%d / %d tags · %s roster", collected, available,
                stats.mgs2_dog_tags_2002 ? "2002" : "2001");
    if (collected != stats.dog_tags) {
        push_font(nullptr, kNoteScale);
        ImGui::TextDisabled("%d saved overall", stats.dog_tags);
        ImGui::PopFont();
    }
    if (begin_fitted_child("mgs2_dog_tags")) {
        apply_scroll(scroll);
        // ponytail: fixed 34 x 394 scan; index only if roster becomes dynamic.
        for (const char* area_code : mgs2::kDogTagAreas) {
            bool header = false;
            for (const auto& tag : mgs2::kDogTags) {
                if (std::strcmp(tag.area, area_code) != 0
                    || !mgs2::dog_tag_available(tag, stats.mission, difficulty)) {
                    continue;
                }
                if (!header) {
                    const char* area = area_name(Game::MGS2, area_code);
                    section_text(area ? area : area_code);
                    header = true;
                }
                const bool done = mgs2::dog_tag_collected(stats.dog_tag_mask, tag.id);
                const mgs2::DogTagGate* gate = mgs2::dog_tag_gate(tag.id);
                const bool state_known = gate && gate->tanker_story_start
                    ? stats.mgs2_t_story != kUnknownMgs2Story
                          && stats.mgs2_t_story <= mgs2::kLastTankerStoryStep
                    : stats.mgs2_p_story != kUnknownMgs2Story
                          && stats.mgs2_p_story <= kLastKnownStoryStep;
                if (!state_known) gate = nullptr;
                const bool gate_ok = !gate || mgs2::dog_tag_gate_available(
                    *gate, stats.mgs2_p_story, stats.mgs2_dog_tag_flags,
                    stats.mgs2_t_story);
                ImVec4 color = done ? id_colors(g_game).green : unset_color();
                if (!done && gate && !gate_ok) {
                    color = gate->kind == mgs2::DogTagGateKind::Until
                            && (!gate->tanker_story_start
                                || stats.mgs2_t_story >= gate->tanker_story_start)
                        ? id_colors(g_game).red : unset_color();
                }
                char suffix[48] = {};
                if (gate) {
                    if (gate->tanker_story_start) {
                        std::snprintf(suffix, sizeof(suffix), "[after Olga fight, before %s]",
                                      gate->trigger);
                    } else {
                        std::snprintf(suffix, sizeof(suffix), "[%s %s]",
                                      gate->kind == mgs2::DogTagGateKind::After ? "after"
                                                                                 : "before",
                                      gate->trigger);
                    }
                }
                const float bluff_scale = tag.bluff ? 1.15f : 1.0f;
                if (tag.bluff) push_font(g_font_bold, bluff_scale);
                push_font(g_font_semibold, bluff_scale);
                ImGui::TextColored(color, "%s", done ? "✓" : "✗");
                ImGui::PopFont();
                ImGui::SameLine();
                ImGui::TextColored(color, "%s", mgs2::dog_tag_name(tag, stats.mgs2_dog_tags_2002));
                if (suffix[0]) {
                    ImGui::SameLine();
                    push_font(nullptr, kNoteScale);
                    ImGui::TextColored(color, "%s", suffix);
                    ImGui::PopFont();
                }
                if (tag.bluff) ImGui::PopFont();
            }
        }
    }
    ImGui::EndChild();
}

bool requirement_mgs2(const codename::ReqStatus& req, const GameStats& stats,
                      char* out, size_t size)
{
    if (std::strcmp(req.label, "radar") == 0) {
        const char* type = stats.radar_type == 0 ? "TYPE-A"
            : stats.radar_type == kRadarTypeB ? "TYPE-B"
            : (stats.radar_type & 4) != 0 ? "OFF" : "?";
        snprintf(out, size, "%s", type);
        return true;
    }
    if (std::strcmp(req.label, "campaign") == 0) {
        const char* selected = stats.mission == mgs2::kPlantMission ? "Plant"
            : stats.mission == mgs2::kTankerMission ? "Tanker"
            : stats.mission == mgs2::kFullCampaignMission ? "Tanker + Plant" : "?";
        snprintf(out, size, "%s", selected);
        return true;
    }
    return false;
}

void summary_extra_mgs2(const GameStats& stats)
{
    summary_count("saved dog tags", stats.dog_tags);
    summary_pair("health", stats.current_health, stats.max_health);
    dim_row("sea louse", stats.sea_louse ? "YES" : "NO");
    summary_count("clearing escapes", stats.clearing_escapes);
    summary_count("times seen", stats.times_seen);
    summary_count("mechs destroyed", stats.mechs_destroyed);
    summary_inline_pair("pull-ups", "Snake", stats.snake_pull_ups, "Raiden",
                        stats.raiden_pull_ups);
}

void summary_area_mgs2(const GameStats& stats, char* out, size_t size)
{
    const char* state = mgs2_area_state(stats);
    if (state) {
        const char* area = mgs2_area_name(stats.area_code);
        if (area) snprintf(out, size, "%s (%s) (%s)", area, state, stats.area_code);
    }
    char stage_time[16];
    format_time(stats.stage_time_seconds, stage_time, sizeof(stage_time));
    const size_t used = std::strlen(out);
    snprintf(out + used, size - used, " · %s", stage_time);
}
