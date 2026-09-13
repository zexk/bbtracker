constexpr IdColors kMgs4Colors{{0.48f, 0.80f, 0.40f, 1}, {0.90f, 0.72f, 0.28f, 1}, {0.88f, 0.30f, 0.24f, 1}};
constexpr const char* kMgs4SpecialItems[] = {"Infinity Bandana", "Stealth Camo"};
constexpr int kFlashbackCount = static_cast<int>(std::size(mgs4::kFlashbacks));
constexpr unsigned kBitsPerByte = 8;

void apply_theme_mgs4(ImGuiStyle& style)
{
    ImVec4* colors = style.Colors;
    style.ScrollbarRounding = 0.0f;
    const ImVec4 text{0.84f, 0.83f, 0.66f, 1.0f};
    const ImVec4 dim{0.49f, 0.48f, 0.36f, 1.0f};
    const ImVec4 panel{0.08f, 0.08f, 0.05f, 1.0f};
    const ImVec4 border{0.52f, 0.48f, 0.31f, 1.0f};
    const ImVec4 frame{0.17f, 0.16f, 0.10f, 1.0f};
    const ImVec4 hover{0.42f, 0.30f, 0.08f, 1.0f};
    const ImVec4 active{0.62f, 0.42f, 0.08f, 1.0f};
    const ImVec4 title{0.13f, 0.12f, 0.07f, 1.0f};
    const ImVec4 title_active{0.32f, 0.27f, 0.12f, 1.0f};
    const ImVec4 accent{0.94f, 0.66f, 0.14f, 1.0f};
    const ImVec4 idle{0.21f, 0.20f, 0.12f, 1.0f};
    const ImVec4 row{0.20f, 0.19f, 0.12f, 1.0f};
    const ImVec4 slider{0.58f, 0.53f, 0.32f, 1.0f};
    const ImVec4 scrollbar{0.38f, 0.35f, 0.21f, 1.0f};
    const ImVec4 scrollbar_bg{0.10f, 0.10f, 0.06f, 1.0f};

    colors[ImGuiCol_Text]                 = text;
    colors[ImGuiCol_TextDisabled]         = dim;
    colors[ImGuiCol_WindowBg]             = with_alpha(panel, kPanelAlpha);
    colors[ImGuiCol_Border]               = with_alpha(border, 0.72f);
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
    colors[ImGuiCol_Separator]            = with_alpha(border, 0.68f);
    colors[ImGuiCol_TableRowBgAlt]        = with_alpha(row, 0.60f);
    colors[ImGuiCol_ResizeGrip]           = with_alpha(active, 0.35f);
    colors[ImGuiCol_ResizeGripHovered]    = with_alpha(accent, 0.75f);
    colors[ImGuiCol_ResizeGripActive]     = accent;
    colors[ImGuiCol_ScrollbarBg]          = with_alpha(scrollbar_bg, 0.85f);
    colors[ImGuiCol_ScrollbarGrab]        = with_alpha(scrollbar, 0.90f);
    colors[ImGuiCol_ScrollbarGrabHovered] = with_alpha(active, 0.95f);
    colors[ImGuiCol_ScrollbarGrabActive]  = accent;
}

const char* mgs4_area_name(const char* code)
{
    static constexpr AreaName kAreas[] = {
        {"s00a00l", "Prologue Cemetery"},
        {"s00a10l", "Ending Cemetery"},
        {"s01a00l", "Middle East Infiltration"},
        {"s01a05l", "Middle East Infiltration"},
        {"s01a10l", "Red Zone"},
        {"s01a20l", "Militia Safehouse"},
        {"s01a30l", "Urban Ruins"},
        {"s01a40l", "Advent Palace"},
        {"s01a50l", "Crescent Meridian"},
        {"s01a55l", "Crescent Meridian"},
        {"s01a57l", "Millennium Park"},
        {"s01a60l", "Liquid's Encampment"},
        {"s02a10l", "Cove Valley Village"},
        {"s02a20l", "Power Station"},
        {"s02a25l", "Power Station"},
        {"s02a30l", "Confinement Facility"},
        {"s02a40l", "Vista Mansion"},
        {"s02a50l", "Research Lab"},
        {"s02a60l", "Mountain Trail / Riverside"},
        {"s02a70l", "Vamp Ambush"},
        {"s02a73l", "Stryker Escape"},
        {"s02a75l", "Stryker Escape"},
        {"s02a78l", "Stryker Escape"},
        {"s02a80l", "High Woodlands Highway"},
        {"s02a85l", "Marketplace Entrance"},
        {"s02a90l", "Marketplace"},
        {"s02a95l", "Marketplace Plaza"},
        {"s03a00l", "Eastern Europe Station"},
        {"s03a10l", "Midtown: Resistance Tail"},
        {"s03a15l", "Midtown: Resistance Tail"},
        {"s03a16l", "Midtown: Canals"},
        {"s03a20l", "Midtown: Plaza"},
        {"s03a25l", "Midtown: North Sector"},
        {"s03a30l", "Church Courtyard"},
        {"s03a35l", "Motorcycle Chase"},
        {"s03a40l", "Motorcycle Chase"},
        {"s03a50l", "Raging Raven Ambush"},
        {"s03a60l", "Motorcycle Chase"},
        {"s03a65l", "Echo's Beacon"},
        {"s03a70l", "Echo's Beacon"},
        {"s03a90l", "Volta River"},
        {"s04a05l", "Metal Gear Solid Flashback"},
        {"s04a10l", "Snowfield / Heliport / Tank Hangar"},
        {"s04a20l", "Nuclear Warhead Storage Building"},
        {"s04a30l", "Snowfield / Communications Tower"},
        {"s04a40l", "Blast Furnace / Casting Facility"},
        {"s04a50l", "Underground Base"},
        {"s04a60l", "Underground Supply Tunnel"},
        {"s04a65l", "REX Escape"},
        {"s04a68l", "Port Area"},
        {"s04a70l", "Port Area: REX vs. RAY"},
        {"s04a75l", "Outer Haven Arrival"},
        {"s05a10l", "Ship Bow"},
        {"s05a20l", "Command Center / Missile Hangar"},
        {"s05a30l", "Microwave Corridor"},
        {"s05a40l", "GW"},
        {"s05a45l", "Liquid Ocelot: Prelude"},
        {"s05a50l", "Liquid Ocelot"},
        {"s05a55l", "Liquid Ocelot: Aftermath"},
        {"s10a10l", "Nomad Mission Briefing"},
        {"s10a20l", "Nomad: South America Briefing"},
        {"s10a30l", "Nomad: Eastern Europe Briefing"},
        {"s10a40l", "Nomad: Shadow Moses Briefing"},
        {"s20a00l", "USS Missouri"},
        {"s20a10l", "USS Missouri vs. Outer Haven"},
        {"s20a20l", "Campbell's Room"},
        {"s30a00l", "Wedding"},
        {"s30a10l", "Hospital"},
    };
    if (const char* area = exact_area_name(code, kAreas)) return area;
    if (std::strncmp(code, "r_sna", 5) == 0) return "Nomad Mission Briefing";
    if (std::strncmp(code, "s00", 3) == 0) return "Prologue";
    if (std::strncmp(code, "s01", 3) == 0) return "Middle East";
    if (std::strncmp(code, "s02", 3) == 0) return "South America";
    if (std::strncmp(code, "s03", 3) == 0) return "Eastern Europe";
    if (std::strncmp(code, "s04", 3) == 0) return "Shadow Moses";
    if (std::strncmp(code, "s05", 3) == 0) return "Outer Haven";
    if (std::strncmp(code, "s06", 3) == 0) return "Epilogue";
    return nullptr;
}

void draw_mgs4_feats(const GameStats& stats, int scroll)
{
    const auto& matches = g_eval.matches;
    const auto matched = [&](const char* name) {
        for (const auto& match : matches) {
            if (std::strcmp(match.name, name) == 0) return true;
        }
        return false;
    };
    if (begin_fitted_child("mgs4_feats")) {
        apply_scroll(scroll);
        if (ImGui::BeginTable("mgs4_feat_rows", 2,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableSetupColumn("emblem", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("progress", ImGuiTableColumnFlags_WidthFixed, ui_size(180));
            const ImVec4 done_color = id_colors(g_game).green;
            const ImVec4 pending_color = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            const auto row = [&](const char* name, const char* value) {
                const bool done = matched(name);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                push_font(g_font_semibold, 1.0f);
                ImGui::TextColored(done ? done_color : pending_color, "%s", name);
                ImGui::PopFont();
                ImGui::TableNextColumn();
                ImGui::TextColored(done ? done_color : pending_color, "%s", value);
            };
            const auto count = [&](const char* name, int value, int target, const char* unit) {
                char text[64];
                snprintf(text, sizeof(text), "%d / %d %s", value, target, unit);
                row(name, text);
            };
            const auto time = [&](const char* name, double seconds, double target_seconds,
                                  const char* unit) {
                char current[16], target[16], text[64];
                format_time(seconds, current, sizeof(current));
                format_time(target_seconds, target, sizeof(target));
                snprintf(text, sizeof(text), "%s / %s %s", current, target, unit);
                row(name, text);
            };
            char text[96];
            // Goals come from shared table so panel cannot drift from rank rules.
            namespace goals = codename::mgs4_goals;
            count("BEAR", stats.cqc_chokes, goals::kBearChokes, "CQC chokes");
            count("EAGLE", stats.headshots, goals::kEagleHeadshots, "headshots");
            snprintf(text, sizeof(text), "knife %d / %d\nCQC %d / %d\nalerts %d / %d",
                      stats.knife_defeats, goals::kAssassinKnife,
                      stats.cqc_holds, goals::kAssassinCqcHolds,
                      stats.alerts, goals::kAssassinMaxAlerts);
            row("ASSASSIN", text);
            snprintf(text, sizeof(text), "%d kills", stats.kills);
            row("PIGEON", text);
            count("BLUE BIRD", stats.items_given, goals::kBlueBirdItems, "items given");
            count("HAWK", stats.praises, goals::kHawkPraises, "praises");
            count("LITTLE GRAY", stats.weapons_acquired, goals::kLittleGrayWeapons, "weapons");
            count("ANT", stats.body_searches, goals::kAntSearches, "searches");
            count("GIBBON", stats.hold_ups, goals::kGibbonHoldUps, "hold-ups");
            time("TORTOISE", stats.box_time_seconds,
                 goals::kTortoiseBoxMinutes * kSecondsPerMinute, "box time");
            count("RABBIT", stats.magazine_pages, goals::kRabbitPages, "pages");
            count("BEE", stats.syringe_uses, goals::kBeeSyringeUses, "uses");
            time("GECKO", stats.wall_time_seconds,
                 goals::kGeckoWallMinutes * kSecondsPerMinute, "wall time");
            count("SCARAB", stats.side_rolls, goals::kScarabSideRolls, "side rolls");
            count("FROG", stats.forward_rolls, goals::kFrogForwardRolls, "forward rolls");
            time("INCH WORM", stats.crawl_time_seconds,
                 goals::kInchWormCrawlMinutes * kSecondsPerMinute, "crawl time");
            time("LOBSTER", stats.crouch_time_seconds,
                 goals::kLobsterCrouchMinutes * kSecondsPerMinute, "crouch time");
            count("HYENA", stats.pickups, goals::kHyenaPickups, "pickups");
            count("HOG", stats.combat_highs, goals::kHogCombatHighs, "combat highs");
            count("PIG", stats.rations_used, goals::kPigRations, "recovery items");
            count("COW", stats.alerts, goals::kCowAlerts, "alerts");
            count("CROCODILE", stats.kills, goals::kCrocodileKills, "kills");
            time("GIANT PANDA", stats.play_time_seconds,
                 goals::kGiantPandaHours * kSecondsPerHour, "play time");
            char current[16], target[16], chicken[192];
            format_time(stats.play_time_seconds, current, sizeof(current));
            format_time(goals::kChickenHours * kSecondsPerHour, target, sizeof(target));
            snprintf(chicken, sizeof(chicken),
                     "alerts %d / %d\nkills %d / %d\ncontinues %d / %d\nrecovery items %d / %d\nplay time %s / %s",
                     stats.alerts, goals::kChickenAlerts,
                     stats.kills, goals::kChickenKills,
                     stats.continues, goals::kChickenContinues,
                     stats.rations_used, goals::kChickenRecoveryItems, current, target);
            row("CHICKEN", chicken);
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
}

void draw_mgs4_flashbacks(const GameStats& stats, int scroll)
{
    char header[48];
    snprintf(header, sizeof(header), "%d / %d unique · %d presses", stats.flashbacks_unique,
             kFlashbackCount,
             stats.flashbacks_viewed);
    ImGui::TextUnformatted(header);
    const ImVec4 done_color = id_colors(g_game).green;
    const ImVec4 todo_color = unset_color();
    if (begin_fitted_child("mgs4_flashbacks")) {
        apply_scroll(scroll);
        for (uint16_t i : mgs4::kFlashbacksStoryOrder) {
            const bool done = (stats.flashback_bits[i / kBitsPerByte]
                               & (1u << (i % kBitsPerByte))) != 0;
            push_font(g_font_semibold, 1.0f);
            ImGui::TextColored(done ? done_color : todo_color, "%s", done ? "✓" : "✗");
            ImGui::PopFont();
            ImGui::SameLine();
            ImGui::TextColored(done ? done_color : todo_color, "%s · %08x",
                               mgs4::kFlashbacks[i].trigger, mgs4::kFlashbacks[i].cue_id);
        }
    }
    ImGui::EndChild();
}

DifficultyDisplay difficulty_mgs4(const GameStats& stats)
{
    const char* names[] = {"Liquid Easy", "Naked Normal", "Solid Normal",
                           "Big Boss Hard", "The Boss Extreme", "?"};
    const size_t index = static_cast<size_t>(stats.difficulty);
    const size_t unknown = std::size(names) - 1;
    return {names[index < unknown ? index : unknown], true,
            stats.difficulty == Difficulty::Extreme
                || stats.difficulty == Difficulty::EuroExtreme};
}

void summary_extra_mgs4(const GameStats& stats)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "%d / %d unique · %d presses", stats.flashbacks_unique,
             kFlashbackCount,
             stats.flashbacks_viewed);
    dim_row("flashbacks", buf);
}
