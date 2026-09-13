constexpr IdColors kMgspwColors{{0.30f, 0.86f, 0.40f, 1}, {0.96f, 0.78f, 0.24f, 1}, {0.95f, 0.11f, 0.14f, 1}};
constexpr unsigned long long kMillisecondsPerSecond = 1000;
constexpr unsigned long long kMillisecondsPerMinute =
    kSecondsPerMinute * kMillisecondsPerSecond;
constexpr int kPwCodenameCount =
    static_cast<int>(std::size(GameStats{}.pw_codename_state)) - 1;

// Peace Walker's own menus are near-black panels with white rules and one
// saturated red for anything selected or failing, and its tabs are the
// only rounded ones in the series.
void apply_theme_mgspw(ImGuiStyle& style)
{
    ImVec4* colors = style.Colors;
    style.CellPadding = ImVec2(6.0f, 3.0f);
    style.TabRounding = 5.0f;
    style.ScrollbarRounding = 0.0f;
    const ImVec4 text{0.93f, 0.93f, 0.90f, 1.0f};
    const ImVec4 dim{0.66f, 0.67f, 0.66f, 1.0f};
    const ImVec4 panel{0.05f, 0.05f, 0.06f, 1.0f};
    const ImVec4 border{0.86f, 0.86f, 0.83f, 1.0f};
    const ImVec4 frame{0.11f, 0.11f, 0.12f, 1.0f};
    const ImVec4 hover{0.55f, 0.05f, 0.07f, 1.0f};
    const ImVec4 active{0.86f, 0.06f, 0.09f, 1.0f};
    const ImVec4 title{0.08f, 0.08f, 0.09f, 1.0f};
    const ImVec4 title_active{0.14f, 0.14f, 0.15f, 1.0f};
    const ImVec4 accent{0.94f, 0.94f, 0.92f, 1.0f};
    const ImVec4 header{0.16f, 0.16f, 0.17f, 1.0f};
    const ImVec4 header_hover{0.24f, 0.24f, 0.25f, 1.0f};
    const ImVec4 header_active{0.32f, 0.32f, 0.33f, 1.0f};
    const ImVec4 slider{0.62f, 0.62f, 0.60f, 1.0f};
    const ImVec4 tab{0.10f, 0.10f, 0.11f, 1.0f};
    const ImVec4 tab_unfocused_active{0.45f, 0.04f, 0.06f, 1.0f};
    const ImVec4 grab_active{0.95f, 0.10f, 0.12f, 1.0f};
    const ImVec4 table_strong{0.60f, 0.60f, 0.58f, 1.0f};
    const ImVec4 table_light{0.40f, 0.40f, 0.39f, 1.0f};
    const ImVec4 row{0.13f, 0.13f, 0.14f, 1.0f};
    const ImVec4 scrollbar{0.32f, 0.32f, 0.33f, 1.0f};
    const ImVec4 scrollbar_bg{0.08f, 0.08f, 0.09f, 1.0f};

    colors[ImGuiCol_Text]                 = text;
    colors[ImGuiCol_TextDisabled]         = dim;
    colors[ImGuiCol_WindowBg]             = with_alpha(panel, kPanelAlpha);
    colors[ImGuiCol_Border]               = with_alpha(border, 0.55f);
    colors[ImGuiCol_FrameBg]              = frame;
    colors[ImGuiCol_FrameBgHovered]       = hover;
    colors[ImGuiCol_FrameBgActive]        = active;
    colors[ImGuiCol_TitleBg]              = title;
    // Red is a selection colour in these menus, not a chrome colour: the
    // title bar stays black so the only red plate is the live tab.
    colors[ImGuiCol_TitleBgActive]        = title_active;
    colors[ImGuiCol_CheckMark]            = accent;
    colors[ImGuiCol_SliderGrab]           = slider;
    colors[ImGuiCol_SliderGrabActive]     = active;
    colors[ImGuiCol_Button]               = frame;
    colors[ImGuiCol_ButtonHovered]        = hover;
    colors[ImGuiCol_ButtonActive]         = active;
    // Table header rows: dark plates under white text, not red ones. Red is
    // kept for the selected tab and for a value that is failing.
    colors[ImGuiCol_Header]               = header;
    colors[ImGuiCol_HeaderHovered]        = header_hover;
    colors[ImGuiCol_HeaderActive]         = header_active;
    colors[ImGuiCol_TableHeaderBg]        = header;
    colors[ImGuiCol_Tab]                  = tab;
    colors[ImGuiCol_TabHovered]           = hover;
    colors[ImGuiCol_TabActive]            = active;
    colors[ImGuiCol_TabUnfocused]         = title;
    colors[ImGuiCol_TabUnfocusedActive]   = tab_unfocused_active;
    colors[ImGuiCol_Separator]            = with_alpha(border, 0.45f);
    colors[ImGuiCol_TableBorderStrong]    = with_alpha(table_strong, 0.55f);
    colors[ImGuiCol_TableBorderLight]     = with_alpha(table_light, 0.45f);
    colors[ImGuiCol_TableRowBgAlt]        = with_alpha(row, 0.55f);
    colors[ImGuiCol_ResizeGrip]           = with_alpha(active, 0.35f);
    colors[ImGuiCol_ResizeGripHovered]    = with_alpha(active, 0.75f);
    colors[ImGuiCol_ResizeGripActive]     = grab_active;
    colors[ImGuiCol_ScrollbarBg]          = with_alpha(scrollbar_bg, 0.80f);
    colors[ImGuiCol_ScrollbarGrab]        = with_alpha(scrollbar, 0.90f);
    colors[ImGuiCol_ScrollbarGrabHovered] = with_alpha(hover, 0.95f);
    colors[ImGuiCol_ScrollbarGrabActive]  = active;
}

const char* mgspw_area_name(const char* code, int region)
{
    if (const char* name = mgspw::region_name(region)) return name;
    static constexpr AreaName kAreas[] = {
        {"w01s03a", "Puerto del Alba"}, {"w01s04a", "El Cenagal: Jungle"},
        {"my_outer", "Mother Base"}, {"my_outer_ap", "Mother Base"},
        {"my_outer_trade", "Mother Base - Trade"}, {"my_outer_tfr", "Mother Base"},
        {"ms_lobby", "Mission Preparation"}, {"result", "Mission Results"},
        {"vs_lobby", "Versus Lobby"}, {"vs_result", "Versus Results"},
        {"title", "Title Screen"}, {"r_title", "Title Screen"},
        {"ending_flow", "Ending"}, {"browser", "Browser"},
    };
    return exact_area_name(code, kAreas);
}

// Live sortie clock and tally.
void draw_mgspw_run(const GameStats& stats)
{
    const auto [id_green, id_yellow, id_red] = id_colors(Game::MGSPW);
    // Poll PW clock each frame for smooth milliseconds.
    const bool results = std::strcmp(stats.pw_stage, "result") == 0;
    uint32_t ticks = results && stats.pw_result_time
        ? stats.pw_result_time : stats.pw_mission_play;
    if (!results && g_clock_fn) {
        g_clock_fn(ticks);
    }
    const unsigned long long mission_ms = static_cast<unsigned long long>(ticks)
        * kMillisecondsPerSecond / kPwTicksPerSecond;
    const unsigned long long best_ms =
        static_cast<unsigned long long>(stats.pw_cur_best)
        * kMillisecondsPerSecond / kPwTicksPerSecond;
    push_font(g_font_semibold, kTitleScale);
    ImGui::Text("%llu:%02llu.%03llu", mission_ms / kMillisecondsPerMinute,
                (mission_ms / kMillisecondsPerSecond) % kSecondsPerMinute,
                mission_ms % kMillisecondsPerSecond);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();

    char buf[64];
    if (stats.pw_mission_id > 0) {
        const char* rank_names[] = {"S", "A", "B", "C"};
        const bool rank_known = stats.pw_cur_rank >= 0
            && stats.pw_cur_rank < static_cast<int>(std::size(rank_names));
        const char* rank = rank_known ? rank_names[stats.pw_cur_rank] : "-";
        // Rank and time are independent save records.
        if (rank_known) {
            push_font(nullptr, kNoteScale);
            ImGui::TextDisabled("Mission %d · Best rank", stats.pw_mission_id);
            ImGui::SameLine(0.0f, ui_size(4.0f));
            push_font(g_font_semibold, 1.0f);
            ImGui::TextDisabled("%s", rank);
            ImGui::PopFont();
            ImGui::PopFont();
        } else {
            push_font(nullptr, kNoteScale);
            ImGui::TextDisabled("Mission %d · No rank recorded", stats.pw_mission_id);
            ImGui::PopFont();
        }
        if (stats.pw_cur_best) {
            push_font(nullptr, kNoteScale);
            ImGui::TextDisabled("Best time %llu:%02llu.%03llu",
                                best_ms / kMillisecondsPerMinute,
                                (best_ms / kMillisecondsPerSecond) % kSecondsPerMinute,
                                best_ms % kMillisecondsPerSecond);
            ImGui::PopFont();
            if (!results) {
                ImGui::SameLine();
                if (mission_ms == best_ms) {
                    ImGui::TextColored(id_yellow, "best reached");
                } else {
                    const double size = std::abs(double(mission_ms) - double(best_ms))
                        / kMillisecondsPerSecond;
                    char gap[24];
                    if (size < kSecondsPerMinute) {
                        snprintf(gap, sizeof(gap), "%.1fs", size);
                    } else {
                        snprintf(gap, sizeof(gap), "%d:%04.1f", int(size) / kSecondsPerMinute,
                                 std::fmod(size, kSecondsPerMinute));
                    }
                    if (mission_ms < best_ms) ImGui::TextDisabled("%s to best", gap);
                    else ImGui::TextColored(id_red, "%s over best", gap);
                }
            }
        } else {
            push_font(nullptr, kNoteScale);
            ImGui::TextDisabled("No best time recorded");
            ImGui::PopFont();
        }
    }
    // Some segment deltas settle at results or lobby exit.
    if (ImGui::BeginTable("pw_current", 2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("this run", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("mission", ImGuiTableColumnFlags_WidthFixed, ui_size(84));
        table_headers_row();
        // Prefer native mission tally; fall back to client-side delta.
        const auto run_stat = [&](const char* k, int mission_value, int seg) {
            if (mission_value >= 0) {
                snprintf(buf, sizeof(buf), "%d", mission_value);
            } else {
                snprintf(buf, sizeof(buf), "%+d (area)", seg);
            }
            stat_row(k, buf);
        };
        // Non-zero kills or alerts forfeit clean-clear insignias.
        const auto clean_row = [&](const char* k, int mission_value, int seg) {
            if (mission_value < 0) {
                run_stat(k, mission_value, seg);
                return;
            }
            snprintf(buf, sizeof(buf), "%d", mission_value);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(k);
            ImGui::TableNextColumn();
            align_value(buf);
            ImGui::TextColored(mission_value ? id_red : id_green, "%s", buf);
        };
        clean_row("kills", stats.pw_m_kills, stats.seg_kills);
        if (stats.pw_m_alerts < 0) unset_row("alerts");
        else clean_row("alerts", stats.pw_m_alerts, 0);
        run_stat("headshots", stats.pw_m_headshots, stats.seg_headshots);
        if (stats.pw_m_holdups < 0) unset_row("hold-ups");
        else run_stat("hold-ups", stats.pw_m_holdups, 0);
        if (stats.pw_m_cqc_uses < 0) unset_row("CQC uses");
        else run_stat("CQC uses", stats.pw_m_cqc_uses, 0);
        if (stats.pw_m_stun_rod_takedowns < 0) unset_row("stun rod KOs");
        else run_stat("stun rod KOs", stats.pw_m_stun_rod_takedowns, 0);
        run_stat("tranq", stats.pw_m_tranq, stats.seg_tranq);
        const bool mission_heroism = stats.pw_m_heroism >= 0;
        snprintf(buf, sizeof(buf), "%+d%s",
                 mission_heroism ? stats.pw_m_heroism : stats.seg_heroism,
                 mission_heroism ? "" : " (area)");
        stat_row("heroism", buf);
        // Full health is the deployed soldier's own maximum, not a constant.
        if (stats.pw_player_max_hp > 0) {
            constexpr int kHpGreenPct = 50;
            constexpr int kHpYellowPct = 25;
            const int hp_pct = stats.pw_player_hp * 100 / stats.pw_player_max_hp;
            snprintf(buf, sizeof(buf), "%d%%", hp_pct);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted("HP");
            ImGui::TableNextColumn();
            align_value(buf);
            ImGui::TextColored(hp_pct >= kHpGreenPct ? id_green : hp_pct >= kHpYellowPct ? id_yellow : id_red,
                               "%s", buf);
        } else {
            unset_row("HP");
        }
        ImGui::EndTable();
    }
}

void draw_mgspw_summary(const GameStats& stats, int)
{
    const char* stage = stats.pw_stage[0] ? stats.pw_stage : "-";
    const char* name = mgspw_area_name(stage, stats.pw_region_id);
    ImGui::Spacing();
    ImGui::PushTextWrapPos(0.0f);
    if (name) {
        ImGui::TextDisabled("%s", name);
    } else {
        ImGui::TextDisabled("%s", stage);
    }
    ImGui::PopTextWrapPos();

    if (stats.pw_in_mission) {
        draw_mgspw_run(stats);
    } else {
        // Later menus may still hold last run's tally. Hide it once results ends.
        ImGui::TextDisabled("No mission running");
        ImGui::Separator();
    }

    ImGui::Spacing();
    const auto& match = g_eval.match;
    ImGui::TextDisabled("Projected codename:");
    ImGui::SameLine(0.0f, ui_size(4.0f));
    push_font(g_font_semibold, 1.0f);
    ImGui::TextDisabled("%s", match ? match->name : "-");
    ImGui::PopFont();
}

void draw_mgspw_global(const GameStats& stats, int scroll)
{
    begin_fitted_child("pw_career_scroll");
    apply_scroll(scroll);
    char buf[64];
    const auto count = [&](const char* label, int value) {
        if (value < 0) {
            unset_row(label);
        } else {
            format_count(value, buf, sizeof(buf));
            stat_row(label, buf);
        }
    };
    if (ImGui::BeginTable("pw_totals", 2, ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("outcome", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("total", ImGuiTableColumnFlags_WidthFixed, ui_size(90));
        count("kills", stats.pw_kills);
        count("non-lethal takedowns", stats.pw_codename_axes_ok
              ? codename::pw_axes(stats).nonlethal : -1);
        ImGui::EndTable();
    }
    ImGui::Spacing();
    if (ImGui::BeginTable("pw_weapons", 3,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("takedowns", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("lethal", ImGuiTableColumnFlags_WidthFixed, ui_size(75));
        ImGui::TableSetupColumn("non-lethal", ImGuiTableColumnFlags_WidthFixed, ui_size(85));
        table_headers_row();
        const auto weapon = [&](const char* label, int slot, int lethal, int nonlethal) {
            if (stats.pw_codename_axes_ok) {
                if (lethal >= 0) lethal = stats.pw_codename_axes[0][slot];
                if (nonlethal >= 0) {
                    nonlethal = stats.pw_codename_axes[1][slot]
                        + stats.pw_codename_axes[2][slot] + stats.pw_codename_axes[3][slot];
                }
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label);
            for (int value : {lethal, nonlethal}) {
                ImGui::TableNextColumn();
                if (value < 0) snprintf(buf, sizeof(buf), "n/a");
                else format_count(value, buf, sizeof(buf));
                align_value(buf);
                if (value < 0) ImGui::TextDisabled("%s", buf);
                else ImGui::TextUnformatted(buf);
            }
        };
        weapon("handguns", 2, stats.pw_pistol_lethal, stats.pw_pistol_takedowns);
        weapon("assault rifles", 3, stats.pw_ar_takedowns, -1);
        weapon("sniper rifles", 4, stats.pw_sniper_takedowns, stats.pw_sniper_nonlethal);
        weapon("machine guns", 5, stats.pw_lmg_takedowns, -1);
        weapon("submachine guns", 6, -1, -1);
        weapon("shotguns", 7, stats.pw_shotgun_takedowns, -1);
        weapon("CQC", 0, -1, stats.pw_cqc_takedowns);
        weapon("stun rod", 1, -1, stats.pw_stun_rod_takedowns);
        weapon("grenades", 9, stats.pw_grenade_takedowns, -1);
        weapon("rockets", 8, stats.pw_rocket_takedowns, -1);
        weapon("placed explosives", 11, stats.pw_placed_takedowns, -1);
        ImGui::EndTable();
    }
    ImGui::Spacing();
    if (ImGui::BeginTable("pw_global", 2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("career", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("total", ImGuiTableColumnFlags_WidthFixed, ui_size(126));
        table_headers_row();
        format_time(stats.pw_total_play, buf, sizeof(buf));
        stat_row("play time", buf);
        char total[16], delta[16];
        format_count(stats.pw_heroism, total, sizeof(total));
        format_count(stats.pw_heroism_delta, delta, sizeof(delta));
        snprintf(buf, sizeof(buf), "%s (%s%s)", total,
                 stats.pw_heroism_delta >= 0 ? "+" : "", delta);
        stat_row("heroism", buf);
        format_count(stats.pw_gmp, buf, sizeof(buf));
        stat_row("GMP", buf);
        count("camaraderie", stats.pw_camaraderie);
        count("clears (with replays)", stats.pw_clears);
        count("unique missions", stats.pw_unique_cleared);
        count("S-ranked missions", stats.pw_s_missions);
        count("headshots", stats.pw_headshots);
        count("alerts", stats.pw_alerts);
        count("unseen kills", stats.pw_stealth_kills);
        count("enemy Fultons", stats.pw_fulton_recoveries);
        count("prisoners extracted", stats.pw_prisoner_extractions);
        count("hold-ups", stats.pw_holdups);
        count("CQC uses", stats.pw_cqc_uses);
        count("no-kill clears", stats.pw_nokill_clears);
        count("no-alert clears", stats.pw_noalert_clears);
        count("no-recovery-item clears", stats.pw_noitem_clears);
        if (stats.pw_damage_taken >= 0) {
            snprintf(buf, sizeof(buf), "%d (%.1f bars)", stats.pw_damage_taken,
                     static_cast<double>(stats.pw_damage_taken) / kPwDamageUnitsPerBar);
            stat_row("damage taken", buf);
        } else {
            unset_row("damage taken");
        }
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::EndChild();
}

void draw_mgspw_insignia(const GameStats& stats, int)
{
    // Each insignia family has three consecutive rising thresholds.
    static constexpr struct {
        const char* label;
        int first_id;
    } kFamilies[] = {
        {"no-alert clears", 1},  {"no-kill clears", 4}, {"hold-ups", 7},
        {"no-item clears", 10},  {"headshots", 16},     {"Fulton recoveries", 44},
    };
    const auto [id_green, id_yellow, id_red] = id_colors(Game::MGSPW);
    if (stats.pw_insignias >= 0) {
        push_font(g_font_semibold, 1.0f);
        ImGui::Text("%d / 110 insignias earned", stats.pw_insignias);
        ImGui::PopFont();
    }
    ImGui::Spacing();
    bool unresolved = false;
    if (ImGui::BeginTable("pw_insignia_stats", 2,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("counter", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("next", ImGuiTableColumnFlags_WidthFixed, ui_size(100));
        table_headers_row();
        const ImVec4 pending = unset_color();
        for (const auto& family : kFamilies) {
            const int have = codename::pw_insignia_progress(family.first_id, stats);
            // Show first unbeaten C/B/A threshold.
            int target = -1;
            const char* tier_name = "";
            for (int tier = 0; tier < 3 && target < 0; ++tier) {
                const int over = codename::pw_insignia(family.first_id + tier).over;
                if (have <= over) {
                    // Grant test is strict.
                    target = over + 1;
                    static constexpr const char* kTiers[] = {"C", "B", "A"};
                    tier_name = kTiers[tier];
                }
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(family.label);
            ImGui::TableNextColumn();
            if (have < 0) {
                unresolved = true;
                ImGui::TextColored(pending, "-");
            } else if (target < 0) {
                ImGui::TextColored(id_green, "%d  done", have);
            } else {
                ImGui::TextColored(have * 4 >= target * 3 ? id_yellow : pending, "%d / %d  %s",
                                   have, target, tier_name);
            }
        }
        ImGui::EndTable();
    }
    if (unresolved) {
        push_font(nullptr, kNoteScale);
        ImGui::TextDisabled("- = counter not resolved yet");
        ImGui::PopFont();
    }
}

void draw_mgspw_codenames(const GameStats& stats, int)
{
    // Weapon distribution decides all-weapons titles.
    const auto [id_green, id_yellow, id_red] = id_colors(Game::MGSPW);
    const codename::PwAxes axes = codename::pw_axes(stats);
    const ImVec4 pending = unset_color();
    const auto& match = g_eval.match;
    const ImVec4 title_color = !match ? pending
        : std::strcmp(match->name, "FOXHOUND") == 0 || std::strcmp(match->name, "FOX") == 0
            ? id_green
            : ImGui::GetStyleColorVec4(ImGuiCol_Text);
    push_font(g_font_semibold, kSectionScale);
    ImGui::TextColored(title_color, "%s", match ? match->name : "---");
    ImGui::PopFont();
    ImGui::SameLine();
    push_font(nullptr, kNoteScale);
    ImGui::TextDisabled("projected");
    if (stats.pw_codename_state_ok) {
        int owned = 0;
        for (int id = 1; id <= kPwCodenameCount; ++id) {
            owned += stats.pw_codename_state[id] & 1;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("· %d / %d earned", owned, kPwCodenameCount);
    }
    ImGui::PopFont();
    if (!axes.native) {
        push_font(nullptr, kNoteScale);
        ImGui::TextDisabled("Estimated from available counters");
        ImGui::PopFont();
    }
    char buf[64];
    ImGui::Spacing();
    if (ImGui::BeginTable("pw_reqs", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("FOX / FOXHOUND", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("have / need", ImGuiTableColumnFlags_WidthFixed, ui_size(100));
        table_headers_row();
        for (const codename::ReqStatus& r : g_eval.reqs) {
            const char* pct =
                static_cast<codename::ReqFmt>(r.fmt) == codename::ReqFmt::Percent ? "%" : "";
            if (r.limit == 0) {
                snprintf(buf, sizeof(buf), "%.0f%s", r.current, pct);
            } else {
                snprintf(buf, sizeof(buf), "%.0f%s / %.0f%s", r.current, pct, r.limit, pct);
            }
            // Unmet goals are pending; exceeded limits fail.
            const auto op = static_cast<codename::Op>(r.op);
            const bool reach = op == codename::Op::Ge || op == codename::Op::Gt;
            const bool near_limit = !reach && r.limit != 0 && r.current >= r.limit * kNearLimitShare;
            // Empty profiles have no spread reading.
            const ImVec4 color = r.pass ? (near_limit ? id_yellow : id_green)
                : !reach                ? (r.current > 0 ? id_red : unset_color())
                : r.limit > 0 && r.current >= r.limit * kCloseGoalShare ? id_yellow
                                                                             : unset_color();
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.label);
            ImGui::TableNextColumn();
            align_value(buf);
            ImGui::TextColored(color, "%s", buf);
        }
        const auto plain = [&](const char* key, int value) {
            format_count(value, buf, sizeof(buf));
            dim_row(key, buf);
        };
        if (stats.pw_camaraderie >= 0) {
            plain("camaraderie", stats.pw_camaraderie);
        }
        ImGui::EndTable();
    }

    if (ImGui::BeginTable("pw_class", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn("takedowns", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("count", ImGuiTableColumnFlags_WidthFixed, ui_size(50));
        ImGui::TableSetupColumn("share", ImGuiTableColumnFlags_WidthFixed, ui_size(50));
        table_headers_row();
        const auto share_row = [&](const char* label, int value, int total,
                                   const ImVec4& share_color) {
            const double share = total > 0 ? 100.0 * value / total : 0.0;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(value > 0 ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : pending,
                               "%s", label);
            ImGui::TableNextColumn();
            // Dim unused classes.
            format_count(value, buf, sizeof(buf));
            align_value(buf);
            if (value > 0) ImGui::TextUnformatted(buf);
            else ImGui::TextDisabled("0");
            ImGui::TableNextColumn();
            if (total > 0) snprintf(buf, sizeof(buf), "%.0f%%", share);
            else snprintf(buf, sizeof(buf), "-");
            align_value(buf);
            ImGui::TextColored(value > 0 ? share_color : pending, "%s", buf);
        };
        for (int cls = 0; cls < 6; ++cls) {
            share_row(codename::pw_class_name(cls), axes.by_class[cls], axes.total, id_yellow);
        }
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("all");
        ImGui::TableNextColumn();
        format_count(axes.total, buf, sizeof(buf));
        align_value(buf);
        ImGui::TextDisabled("%s", buf);
        ImGui::TableNextColumn();
        align_value(axes.total > 0 ? "100%" : "-");
        ImGui::TextDisabled("%s", axes.total > 0 ? "100%" : "-");
        // Lethality is another view of same takedowns.
        ImGui::TableNextRow(ImGuiTableRowFlags_None, ui_size(6));
        // FOXHOUND is a non-lethal title: non-lethal must beat twice lethal.
        share_row("lethal", axes.lethal, axes.lethal + axes.nonlethal, id_red);
        share_row("non-lethal", axes.nonlethal, axes.lethal + axes.nonlethal,
                  codename::pw_nonlethal_beats_lethal(axes.lethal, axes.nonlethal) ? id_green
                                                                                   : id_red);
        ImGui::EndTable();
    }

    ImGui::Spacing();
    const codename::PwGrade grade = codename::pw_grade(stats);
    const bool coop = stats.pw_camaraderie > codename::kPwCoopCamaraderie;
    // Show first failing grade, held grade 5, or initial grade 1.
    const int target = grade.next ? grade.next : grade.grade ? grade.grade : 1;
    const codename::PwGradeGate gate = codename::pw_grade_gate(target);
    const bool ratio_known = stats.pw_codename_result_ok
        && stats.pw_codename_missions_required > 0
        && stats.pw_codename_missions_counted >= 0;
    if (!stats.pw_codename_result_ok) {
        // Before evaluation, show grade 1 gates without status.
        push_font(nullptr, kNoteScale);
        ImGui::TextDisabled("No grade yet · finish a mission to be evaluated");
        ImGui::PopFont();
    } else if (grade.next) {
        push_font(g_font_semibold, 1.0f);
        ImGui::TextColored(grade.grade ? id_green : pending, "Grade %d / 5", grade.grade);
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::TextColored(id_yellow, "→ next %d", grade.next);
    } else {
        push_font(g_font_semibold, 1.0f);
        ImGui::TextColored(id_green, "Grade 5 / 5 · highest grade held");
        ImGui::PopFont();
    }
    char header[32];
    snprintf(header, sizeof(header), "grade %d needs", target);
    if (ImGui::BeginTable("pw_ladder", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableSetupColumn(header, ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("now", ImGuiTableColumnFlags_WidthFixed, ui_size(70));
        ImGui::TableSetupColumn("goal", ImGuiTableColumnFlags_WidthFixed, ui_size(70));
        table_headers_row();
        // Unread or irrelevant gates have no failure status.
        const auto gate_row = [&](const char* label, bool known, bool judged,
                                  const char* now, const char* goal, bool pass) {
            const ImVec4 color = !known || !judged ? pending : pass ? id_green : id_red;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(judged ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : pending,
                               "%s", label);
            ImGui::TableNextColumn();
            align_value(known ? now : "-");
            ImGui::TextColored(color, "%s", known ? now : "-");
            ImGui::TableNextColumn();
            align_value(goal);
            ImGui::TextColored(color, "%s", goal);
        };
        char now_value[24], goal_value[32], gate_text[24];
        // Co-op requires above step; solo requires at or below.
        format_thousands(gate.camaraderie, gate_text, sizeof(gate_text));
        snprintf(goal_value, sizeof(goal_value), "%s%s", coop ? ">" : "<=", gate_text);
        format_thousands(stats.pw_camaraderie, now_value, sizeof(now_value));
        gate_row("camaraderie", stats.pw_camaraderie >= 0, true, now_value, goal_value,
                 coop ? stats.pw_camaraderie > gate.camaraderie
                      : stats.pw_camaraderie <= gate.camaraderie);
        // Only all-weapons titles (FOXHOUND among them) carry the Heroism floor.
        if (grade.all_weapons) {
            format_thousands(gate.heroism, gate_text, sizeof(gate_text));
            snprintf(goal_value, sizeof(goal_value), ">%s", gate_text);
        } else {
            snprintf(goal_value, sizeof(goal_value), "any");
        }
        format_thousands(stats.pw_heroism, now_value, sizeof(now_value));
        gate_row("heroism", true, grade.all_weapons, now_value, goal_value,
                 stats.pw_heroism > gate.heroism);
        // Grade 3 and up want the ratio to reach the gate, 1 and 2 to pass it.
        const double ratio = ratio_known
            ? static_cast<double>(stats.pw_codename_missions_counted)
                / stats.pw_codename_missions_required
            : 0.0;
        snprintf(now_value, sizeof(now_value), "%.2f", ratio);
        snprintf(goal_value, sizeof(goal_value), "%s%.2f", target >= 3 ? ">=" : ">",
                 gate.coop_ratio);
        gate_row("co-op ratio", ratio_known, true, now_value, goal_value,
                 target >= 3 ? ratio >= gate.coop_ratio : ratio > gate.coop_ratio);
        // Grades 4-5 also require native mission-rank verdict.
        if (target >= 4) {
            const bool flag = target == 5 ? stats.pw_codename_grade5_ok
                                          : stats.pw_codename_grade4_ok;
            gate_row("mission ranks", stats.pw_codename_result_ok, true,
                     flag ? "met" : "not met", "met", flag);
        }
        ImGui::EndTable();
    }
    if (ratio_known) {
        push_font(nullptr, kNoteScale);
        ImGui::TextDisabled("ratio = %d / %d missions counted",
                            stats.pw_codename_missions_counted,
                            stats.pw_codename_missions_required);
        ImGui::PopFont();
    }
}
