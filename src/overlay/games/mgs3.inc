constexpr IdColors kMgs3Colors{{0.55f, 0.78f, 0.42f, 1}, {0.88f, 0.72f, 0.28f, 1}, {0.82f, 0.32f, 0.24f, 1}};
constexpr const char* kMgs3SpecialItems[] = {
    "Stealth Camo", "Infinity Face Paint", "EZ Gun"};
constexpr int kCaptureCount = static_cast<int>(std::size(mgs3::kCaptures));
constexpr int kKerotanCount = static_cast<int>(std::size(mgs3::kKerotans));

void apply_theme_mgs3(ImGuiStyle& style)
{
    ImVec4* colors = style.Colors;
    const ImVec4 text{0.86f, 0.87f, 0.76f, 1.0f};
    const ImVec4 dim{0.48f, 0.49f, 0.42f, 1.0f};
    const ImVec4 panel{0.09f, 0.10f, 0.08f, 1.0f};
    const ImVec4 border{0.38f, 0.39f, 0.31f, 1.0f};
    const ImVec4 frame{0.20f, 0.21f, 0.17f, 1.0f};
    const ImVec4 frame_hover{0.29f, 0.30f, 0.24f, 1.0f};
    const ImVec4 frame_active{0.36f, 0.37f, 0.29f, 1.0f};
    const ImVec4 hover{0.36f, 0.37f, 0.28f, 1.0f};
    const ImVec4 active{0.44f, 0.45f, 0.33f, 1.0f};
    const ImVec4 title{0.15f, 0.16f, 0.12f, 1.0f};
    const ImVec4 title_active{0.31f, 0.33f, 0.25f, 1.0f};
    const ImVec4 accent{0.72f, 0.75f, 0.51f, 1.0f};
    const ImVec4 idle{0.25f, 0.26f, 0.20f, 1.0f};
    const ImVec4 slider{0.55f, 0.57f, 0.41f, 1.0f};
    const ImVec4 row{0.22f, 0.23f, 0.19f, 1.0f};
    const ImVec4 scrollbar{0.38f, 0.39f, 0.28f, 1.0f};
    const ImVec4 scrollbar_bg{0.12f, 0.13f, 0.10f, 1.0f};

    style.ScrollbarRounding = 0.0f;
    colors[ImGuiCol_Text]                 = text;
    colors[ImGuiCol_TextDisabled]         = dim;
    colors[ImGuiCol_WindowBg]             = with_alpha(panel, kPanelAlpha);
    colors[ImGuiCol_Border]               = with_alpha(border, 0.65f);
    colors[ImGuiCol_FrameBg]              = frame;
    colors[ImGuiCol_FrameBgHovered]       = frame_hover;
    colors[ImGuiCol_FrameBgActive]        = frame_active;
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
    colors[ImGuiCol_Separator]            = with_alpha(border, 0.65f);
    colors[ImGuiCol_TableRowBgAlt]        = with_alpha(row, 0.55f);
    colors[ImGuiCol_ResizeGrip]           = with_alpha(slider, 0.30f);
    colors[ImGuiCol_ResizeGripHovered]    = with_alpha(accent, 0.70f);
    colors[ImGuiCol_ResizeGripActive]     = accent;
    colors[ImGuiCol_ScrollbarBg]          = with_alpha(scrollbar_bg, 0.70f);
    colors[ImGuiCol_ScrollbarGrab]        = with_alpha(scrollbar, 0.85f);
    colors[ImGuiCol_ScrollbarGrabHovered] = with_alpha(slider, 0.90f);
    colors[ImGuiCol_ScrollbarGrabActive]  = with_alpha(accent, 0.95f);
}

const char* mgs3_area_name(const char* code)
{
    static constexpr AreaName kAreas[] = {
        {"v000a", "Dremuchij South (before backpack)"},
        {"v001a", "Dremuchij South (VM)"}, {"v003a", "Dremuchij Swampland (VM)"},
        {"v004a", "Dremuchij North (VM)"}, {"v005a", "Dolinovodno (VM)"},
        {"v006a", "Rassvet (before Ocelot)"}, {"v006b", "Rassvet (after Ocelot)"},
        {"v007a", "Dolinovodno Riverbank"},
        {"s001a", "Dremuchij South"}, {"s002a", "Dremuchij East"},
        {"s003a", "Dremuchij Swampland"}, {"s004a", "Dremuchij North"},
        {"s005a", "Dolinovodno"}, {"s006a", "Rassvet (before EVA)"},
        {"s006b", "Rassvet"}, {"s012a", "Chyornyj Prud"},
        {"s021a", "Bolshaya Past South"}, {"s022a", "Bolshaya Past Base"},
        {"s023a", "Bolshaya Past Crevice"},
        {"s031a", "Chyornaya Peschera Cave Branch"},
        {"s032a", "Chyornaya Peschera Cave"},
        {"s032b", "Chyornaya Peschera Cave (The Pain)"},
        {"s033a", "Chyornaya Peschera Cave Entrance"},
        {"s041a", "Ponizovje South"}, {"s042a", "Ponizovje West"},
        {"s043a", "Ponizovje Warehouse Exterior"},
        {"s044a", "Ponizovje Warehouse"}, {"s045a", "Svyatogornyj South"},
        {"s051a", "Graniny Gorki South"}, {"s051b", "Graniny Gorki South (The Fear)"},
        {"s052a", "Graniny Gorki Lab Exterior: Outside Walls"},
        {"s052b", "Graniny Gorki Lab Exterior: Inside Walls"},
        {"s053a", "Graniny Gorki Lab 1F/2F"},
        {"s055a", "Graniny Gorki Lab B1 East"},
        {"s056a", "Graniny Gorki Lab B1 West"},
        {"s061a", "Svyatogornyj West"}, {"s062a", "Svyatogornyj East"},
        {"s063a", "Sokrovenno South (The End)"},
        {"s063b", "Sokrovenno South (Ocelot Unit)"},
        {"s064a", "Sokrovenno West (The End)"},
        {"s064b", "Sokrovenno West (Ocelot Unit)"},
        {"s065a", "Sokrovenno North (The End)"},
        {"s065b", "Sokrovenno North (Ocelot Unit)"},
        {"s066a", "Krasnogorje Tunnel"}, {"s071a", "Krasnogorje Mountain Base"},
        {"s072a", "Krasnogorje Mountainside"}, {"s072b", "Krasnogorje Mountainside"},
        {"s073a", "Krasnogorje Mountaintop (before EVA)"},
        {"s073b", "Krasnogorje Mountaintop"},
        {"s074a", "Krasnogorje Mountaintop Ruins"},
        {"s075a", "Krasnogorje Mountaintop: Behind Ruins"},
        {"s081a", "Groznyj Grad Underground Tunnel"},
        {"s091a", "Groznyj Grad Southwest (before torture)"},
        {"s091b", "Groznyj Grad Southwest (prison escape)"},
        {"s091c", "Groznyj Grad Southwest"},
        {"s092a", "Groznyj Grad Northwest (before torture)"},
        {"s092b", "Groznyj Grad Northwest (prison escape)"},
        {"s092c", "Groznyj Grad Northwest"},
        {"s093a", "Groznyj Grad Northeast (before torture)"},
        {"s093b", "Groznyj Grad Northeast (prison escape)"},
        {"s093c", "Groznyj Grad Northeast"},
        {"s094a", "Groznyj Grad Southeast (before torture)"},
        {"s094b", "Groznyj Grad Southeast (prison escape)"},
        {"s094c", "Groznyj Grad Southeast"},
        {"s101a", "Weapons Lab East Wing (with Raikov)"},
        {"s101b", "Weapons Lab East Wing"},
        {"s111a", "Weapons Lab West Wing Corridor"},
        {"s112a", "Groznyj Grad Torture Room"}, {"s113a", "Groznyj Grad Sewers"},
        {"s121a", "Weapons Lab Main Wing"},
        {"s122a", "Weapons Lab Main Wing B1 (Volgin)"},
        {"s141a", "The Sorrow's River"}, {"s151a", "Tikhogornyj"},
        {"s152a", "Tikhogornyj: Behind Waterfall"}, {"s161a", "Groznyj Grad"},
        {"s162a", "Groznyj Grad Runway South"}, {"s163a", "Groznyj Grad Runway"},
        {"s163b", "Groznyj Grad Runway (Shagohod)"},
        {"s171a", "Groznyj Grad Rail Bridge"},
        {"s171b", "Groznyj Grad Rail Bridge (Shagohod)"},
        {"s181a", "Groznyj Grad Rail Bridge North"},
        {"s182a", "Lazorevo South"}, {"s183a", "Lazorevo North"},
        {"s191a", "Zaozyorje West (before patrol)"}, {"s191b", "Zaozyorje West"},
        {"s192a", "Zaozyorje East"}, {"s201a", "Rokovoj Bereg"},
        {"s211a", "WIG Interior"},
    };
    return exact_area_name(code, kAreas);
}

void draw_mgs3_captures(const GameStats& stats, int scroll)
{
    ImGui::Text("%d / %d", stats.plants_captured, kCaptureCount);
    checklist("captures", mgs3::kCaptures, nullptr, std::size(mgs3::kCaptures), stats.capture_mask, scroll);
}

void draw_mgs3_kerotans(const GameStats& stats, int scroll)
{
    ImGui::Text("%d / %d", stats.kerotans, kKerotanCount);
    checklist("kerotans", mgs3::kKerotans, mgs3::kKerotanBits, std::size(mgs3::kKerotans),
              stats.kerotan_mask, scroll);
}

void summary_extra_mgs3(const GameStats& stats)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d / %d", stats.plants_captured, kCaptureCount);
    dim_row("captures", buf);
    const int current_kerotan = mgs3::area_kerotan(stats.area_code);
    snprintf(buf, sizeof(buf), "%d / %d  %s", stats.kerotans, kKerotanCount,
             current_kerotan >= 0
                 && (stats.kerotan_mask & (uint64_t{1} << current_kerotan)) != 0
                 ? "✓" : "✗");
    dim_row("kerotans", buf);
    summary_count("meals eaten", stats.meals_eaten);
}
