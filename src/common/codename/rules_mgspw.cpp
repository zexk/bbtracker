// Peace Walker codename rules.
//
// PW does not score a codename from thresholds the way MGS1-4 do. It
// classifies career play on three axes and hands out the matching title from a
// 24-entry table; the table below is the game's own, recovered from
// localization block slot/001FC/3 (names at 0-23, descriptions at 24+index).
//
// Native evaluator is mapped in docs/mgspw_research.md. Exact axes are used
// when probe resolves them; older partial counters remain fallback.

#include "codename.h"

#include <array>
#include <cmath>

namespace bb::codename {

namespace {

enum class WeaponClass : uint8_t { Short, Medium, Long, Explosive, Cqc, Stun, All };

struct PwTitle {
    const char* name;
    WeaponClass cls;
    bool coop;
    bool nonlethal;
};

// Index is the game's own title index; FOX (14) is the solo counterpart to
// FOXHOUND and the only slot whose name string the extraction did not carry.
constexpr std::array<PwTitle, 24> kPwTitles{{
    {"WOLF",      WeaponClass::Medium,    true,  false},
    {"WHALE",     WeaponClass::Explosive, true,  true},
    {"SWALLOW",   WeaponClass::Long,      false, true},
    {"SCORPION",  WeaponClass::Short,     false, false},
    {"RAVEN",     WeaponClass::Long,      true,  false},
    {"PUMA",      WeaponClass::Medium,    false, false},
    {"PIRANHA",   WeaponClass::Explosive, true,  false},
    {"ORCA",      WeaponClass::Explosive, false, false},
    {"OCTOPUS",   WeaponClass::Explosive, false, true},
    {"KANGAROO",  WeaponClass::Cqc,       true,  true},
    {"HOUND",     WeaponClass::All,       false, false},
    {"HAWK",      WeaponClass::Long,      false, false},
    {"GULL",      WeaponClass::Long,      true,  true},
    {"FOXHOUND",  WeaponClass::All,       true,  true},
    {"FOX",       WeaponClass::All,       false, true},
    {"FIREFLY",   WeaponClass::Stun,      true,  true},
    {"EEL",       WeaponClass::Stun,      false, true},
    {"DOBERMAN",  WeaponClass::All,       true,  false},
    {"DEER",      WeaponClass::Medium,    true,  true},
    {"CAT",       WeaponClass::Medium,    false, true},
    {"BUTTERFLY", WeaponClass::Short,     false, true},
    {"BEE",       WeaponClass::Short,     true,  false},
    {"BEAR",      WeaponClass::Cqc,       false, true},
    {"ANT",       WeaponClass::Short,     true,  true},
}};

// A counter reads -1 until its descriptor id resolves; treat that as zero so a
// partially resolved profile still classifies instead of vanishing.
constexpr int pw_count(int value)
{
    return value > 0 ? value : 0;
}

struct PwProfile {
    int by_class[6] = {};
    int total = 0;
    int top = 0;
    int classes_used = 0;
    WeaponClass dominant = WeaponClass::All;
    int nonlethal = 0;
    int lethal = 0;
    bool balanced = false;
};

// "All weapons" is the elite branch: no class may hold this share or more of
// career takedowns, and this many classes must be in use.
constexpr double kPwSpreadShare = 0.40;
constexpr int kPwSpreadClasses = 4;

PwProfile pw_profile(const GameStats& s)
{
    PwProfile p;
    if (s.pw_codename_axes_ok) {
        int slots[12]{};
        for (int slot = 0; slot < 12; ++slot) {
            for (int axis = 0; axis < 4; ++axis) {
                slots[slot] += pw_count(s.pw_codename_axes[axis][slot]);
            }
            p.total += slots[slot];
            if (slots[slot] > p.top) p.top = slots[slot];
            p.lethal += pw_count(s.pw_codename_axes[0][slot]);
            p.nonlethal += pw_count(s.pw_codename_axes[1][slot])
                + pw_count(s.pw_codename_axes[2][slot])
                + pw_count(s.pw_codename_axes[3][slot]);
        }
        unsigned dominant_mask = 0;
        for (int slot = 0; slot < 12; ++slot) {
            if (slots[slot] == p.top) dominant_mask |= 1u << slot;
        }
        constexpr struct { unsigned mask; WeaponClass cls; } groups[] = {
            {0x002, WeaponClass::Stun}, {0x001, WeaponClass::Cqc},
            {0x1B00, WeaponClass::Explosive}, {0x010, WeaponClass::Long},
            {0x028, WeaponClass::Medium}, {0x0C4, WeaponClass::Short},
        };
        for (const auto& group : groups) {
            if (dominant_mask & group.mask) {
                p.dominant = group.cls;
                break;
            }
        }
        const double average = static_cast<double>(p.total) / 11.0;
        p.balanced = true;
        for (int value : slots) {
            if (std::abs(static_cast<double>(value) - average) > average * 0.1) {
                p.balanced = false;
                break;
            }
        }
        if (p.balanced) p.dominant = WeaponClass::All;
        return p;
    }
    p.by_class[static_cast<int>(WeaponClass::Short)] =
        pw_count(s.pw_pistol_takedowns) + pw_count(s.pw_pistol_lethal)
        + pw_count(s.pw_shotgun_takedowns);
    p.by_class[static_cast<int>(WeaponClass::Medium)] =
        pw_count(s.pw_ar_takedowns) + pw_count(s.pw_lmg_takedowns);
    p.by_class[static_cast<int>(WeaponClass::Long)] =
        pw_count(s.pw_sniper_takedowns) + pw_count(s.pw_sniper_nonlethal);
    p.by_class[static_cast<int>(WeaponClass::Explosive)] =
        pw_count(s.pw_grenade_takedowns) + pw_count(s.pw_rocket_takedowns)
        + pw_count(s.pw_placed_takedowns);
    p.by_class[static_cast<int>(WeaponClass::Cqc)] = pw_count(s.pw_cqc_takedowns);
    p.by_class[static_cast<int>(WeaponClass::Stun)] = 0; // no stun-rod id yet

    int top_index = 0;
    for (int i = 0; i < 6; ++i) {
        p.total += p.by_class[i];
        if (p.by_class[i] > 0) ++p.classes_used;
        if (p.by_class[i] > p.top) {
            p.top = p.by_class[i];
            top_index = i;
        }
    }
    const bool spread = p.total > 0
        && p.classes_used >= kPwSpreadClasses
        && static_cast<double>(p.top) < static_cast<double>(p.total) * kPwSpreadShare;
    p.balanced = spread;
    p.dominant = spread ? WeaponClass::All : static_cast<WeaponClass>(top_index);
    p.nonlethal = pw_count(s.pw_tranq) + pw_count(s.pw_cqc_takedowns);
    p.lethal = pw_count(s.pw_kills);
    return p;
}

} // namespace

// Insignias, by the game's own id. Thresholds and Heroism awards are the
// .rdata table 0x140543810 assembles on the stack; names are the localization
// rows those ids resolve to. scripts/pwinsig.py and scripts/pwolang.py
// regenerate both halves from the install.
constexpr PwInsignia kPwInsignias[110] = {
    {"Stealth Master (Rank C)", 25, 500},
    {"Stealth Master (Rank B)", 50, 2500},
    {"Stealth Master (Rank A)", 100, 5000},
    {"Master of Non-Lethal Force (Rank C)", 50, 500},
    {"Master of Non-Lethal Force (Rank B)", 100, 2500},
    {"Master of Non-Lethal Force (Rank A)", 200, 5000},
    {"Hold-Up Master (Rank C)", 100, 1000},
    {"Hold-Up Master (Rank B)", 250, 5000},
    {"Hold-Up Master (Rank A)", 500, 10000},
    {"No-Recovery Master (Rank C)", 50, 500},
    {"No-Recovery Master (Rank B)", 100, 2500},
    {"No-Recovery Master (Rank A)", 200, 5000},
    {"CQC Master (Rank C)", 100, 500},
    {"CQC Master (Rank B)", 500, 2500},
    {"CQC Master (Rank A)", 1000, 5000},
    {"Headshot Master (Rank C)", 100, 500},
    {"Headshot Master (Rank B)", 500, 2500},
    {"Headshot Master (Rank A)", 1000, 5000},
    {"CO-OPS Specialist (Rank C)", 250000, 1500},
    {"CO-OPS Specialist (Rank B)", 500000, 5500},
    {"CO-OPS Specialist (Rank A)", 1000000, 12000},
    {"Medic Master (Rank C)", 50, 500},
    {"Medic Master (Rank B)", 100, 2500},
    {"Medic Master (Rank A)", 200, 5000},
    {"CO-OP In Specialist", 250, 500},
    {"Snake Formation Specialist", 250, 1250},
    {"Sync Specialist", 250, 2500},
    {"Item Sharing Specialist", 100, 2500},
    {"Love Box Specialist", 2, 2500},
    {"Personnel Manager", 100, 2500},
    {"Present Enthusiast", 50, 2500},
    {"Mission Completion Specialist (Rank C)", -1, 2500},
    {"Mission Completion Specialist (Rank B)", -1, 5000},
    {"Mission Completion Specialist (Rank A)", -1, 10000},
    {"OUTER OPS Specialist (Rank C)", 25, 500},
    {"OUTER OPS Specialist (Rank B)", 50, 2500},
    {"OUTER OPS Specialist (Rank A)", 100, 5000},
    {"Mission Completion Pro", 500, 3000},
    {"Enemy Search Specialist", 1000, 500},
    {"Cardboard Box Lover", 3, 500},
    {"Photo Specialist", 500, 500},
    {"Naked Enthusiast", 200, 500},
    {"CO-OPS Messages Specialist", 500, 500},
    {"Fulton Recovery Specialist (Rank C)", 100, 250},
    {"Fulton Recovery Specialist (Rank B)", 500, 1250},
    {"Fulton Recovery Specialist (Rank A)", 1000, 2500},
    {"Parts Recovery Specialist (Rank C)", 15, 250},
    {"Parts Recovery Specialist (Rank B)", 30, 1250},
    {"Parts Recovery Specialist (Rank A)", 49, 2500},
    {"Monster Hunting Specialist (Rank C)", 50, 250},
    {"Monster Hunting Specialist (Rank B)", 100, 1250},
    {"Monster Hunting Specialist (Rank A)", 200, 2500},
    {"Insignia Collection Specialist (Rank C)", 10, 1000},
    {"Insignia Collection Specialist (Rank B)", 50, 7250},
    {"Insignia Collection Specialist (Rank A)", 100, 20000},
    {"Codename Collection Specialist (Rank C)", 10, 1000},
    {"Codename Collection Specialist (Rank B)", 50, 7250},
    {"Codename Collection Specialist (Rank A)", 100, 20000},
    {"WAC Coordinator", 300, 2500},
    {"Item Creator", -1, 1000},
    {"Weapon Creator", -1, 1000},
    {"Mech Hunting Specialist", -1, 1000},
    {"Handgun Specialist", -1, 2500},
    {"Submachine Gun Specialist", -1, 2500},
    {"Shotgun Specialist", -1, 2500},
    {"Assault Rifle Specialist", -1, 2500},
    {"Sniper Rifle Specialist", -1, 2500},
    {"Machine Gun Specialist", -1, 2500},
    {"Missile Specialist", -1, 2500},
    {"Grenade Specialist", -1, 2500},
    {"Explosives Specialist", -1, 2500},
    {"Stun Rod Specialist", -1, 200},
    {"Weapon Master", -1, 5000},
    {"Mother Base Development Specialist", -1, 1000},
    {"Mother Base Master", -1, 5000},
    {"VS Rookie", -1, 50},
    {"VS Enthusiast (Rank C)", 50, 250},
    {"VS Enthusiast (Rank B)", 100, 1250},
    {"VS Enthusiast (Rank A)", 250, 2500},
    {"???", 50, 250},
    {"???", 100, 1250},
    {"???", 200, 2500},
    {"VS Master (Rank C)", 50, 250},
    {"VS Master (Rank B)", 100, 1250},
    {"VS Master (Rank A)", 200, 2500},
    {"???", 10, 500},
    {"???", 50, 2500},
    {"???", 100, 5000},
    {"Deathmatch Master", 50, 2500},
    {"Team Deathmatch Master", 50, 2500},
    {"Base Master", 50, 2500},
    {"Capture Pro", 50, 2500},
    {"VS Battle Master (Rank C)", 100, 250},
    {"VS Battle Master (Rank B)", 250, 1250},
    {"VS Battle Master (Rank A)", 500, 2500},
    {"VS Kill Streak Master", 5, 250},
    {"VS Kill Streak Master", 10, 1250},
    {"VS Kill Streak Master", 20, 2500},
    {"VS Headshot Streak Master", 5, 250},
    {"VS Headshot Streak Master", 10, 1250},
    {"VS Headshot Streak Master", 15, 2500},
    {"Base Specialist", 10, 250},
    {"Base Specialist", 25, 1250},
    {"Base Specialist", 50, 2500},
    {"Capture Pro", 10, 250},
    {"Capture Pro", 25, 1250},
    {"Capture Pro", 50, 2500},
    {"VS Fulton Recovery Specialist (Rank C)", 10, 250},
    {"VS Fulton Recovery Specialist (Rank B)", 50, 1250},
    {"VS Fulton Recovery Specialist (Rank A)", 100, 2500},
};

PwInsignia pw_insignia(int id)
{
    if (id < 1 || id > 110) {
        return {"?", -1, 0};
    }
    return kPwInsignias[id - 1];
}

int pw_insignia_progress(int id, const GameStats& s)
{
    switch (id) {
    case 1: case 2: case 3:     return s.pw_noalert_clears;      // Stealth Master
    case 4: case 5: case 6:     return s.pw_nokill_clears;       // Non-Lethal Force
    case 7: case 8: case 9:     return s.pw_holdups;             // Hold-Up Master
    case 10: case 11: case 12:  return s.pw_noitem_clears;       // No-Recovery Master
    case 16: case 17: case 18:  return s.pw_headshots;           // Headshot Master
    case 44: case 45: case 46:  return s.pw_fulton_recoveries;   // Fulton Recovery
    default: return -1;
    }
}

namespace {

// Gates per grade 1..5, from the native evaluator. Camaraderie steps are the
// same numbers for both variants: a solo (low) title needs camaraderie at or
// below its step, a cooperation title strictly above it. All-weapons titles
// add a Heroism floor on top.
constexpr int kPwCamaraderieStep[] = {10000, 50000, 100000, 200000, 500000};
constexpr int kPwHeroismFloor[] = {10000, 50000, 100000, 150000, 250000};
constexpr double kPwCoopRatioGate[] = {0.05, 0.5, 1.0, 1.0, 1.0};

double pw_coop_ratio(const GameStats& s)
{
    return s.pw_codename_missions_required > 0
        ? static_cast<double>(s.pw_codename_missions_counted)
            / static_cast<double>(s.pw_codename_missions_required)
        : 0.0;
}

} // namespace

// The twelve slots the evaluator reads, mapped onto the six title classes.
// Taken from the dominance masks in pw_profile; slot 10 is in none of them.
constexpr int kPwSlotClass[12] = {
    static_cast<int>(WeaponClass::Cqc),       // 0
    static_cast<int>(WeaponClass::Stun),      // 1
    static_cast<int>(WeaponClass::Short),     // 2
    static_cast<int>(WeaponClass::Medium),    // 3
    static_cast<int>(WeaponClass::Long),      // 4
    static_cast<int>(WeaponClass::Medium),    // 5
    static_cast<int>(WeaponClass::Short),     // 6
    static_cast<int>(WeaponClass::Short),     // 7
    static_cast<int>(WeaponClass::Explosive), // 8
    static_cast<int>(WeaponClass::Explosive), // 9
    -1,                                       // 10
    static_cast<int>(WeaponClass::Explosive), // 11
};

const char* pw_class_name(int cls)
{
    static constexpr const char* kNames[] = {
        "short", "medium", "long", "explosive", "CQC", "stun rod"};
    return cls >= 0 && cls < 6 ? kNames[cls] : "?";
}

PwGradeGate pw_grade_gate(int grade)
{
    if (grade < 1 || grade > 5) {
        return {0, 0, 0.0};
    }
    const int i = grade - 1;
    return {kPwCamaraderieStep[i], kPwHeroismFloor[i], kPwCoopRatioGate[i]};
}

PwAxes pw_axes(const GameStats& s)
{
    PwAxes out;
    if (s.pw_codename_axes_ok) {
        out.native = true;
        for (int slot = 0; slot < 12; ++slot) {
            for (int axis = 0; axis < 4; ++axis) {
                out.slot[slot] += pw_count(s.pw_codename_axes[axis][slot]);
            }
            out.lethal += pw_count(s.pw_codename_axes[0][slot]);
            out.nonlethal += pw_count(s.pw_codename_axes[1][slot])
                + pw_count(s.pw_codename_axes[2][slot])
                + pw_count(s.pw_codename_axes[3][slot]);
            if (kPwSlotClass[slot] >= 0) {
                out.by_class[kPwSlotClass[slot]] += out.slot[slot];
            }
        }
    } else {
        const PwProfile p = pw_profile(s);
        for (int i = 0; i < 6; ++i) {
            out.by_class[i] = p.by_class[i];
        }
        out.lethal = p.lethal;
        out.nonlethal = p.nonlethal;
    }
    for (int i = 0; i < 6; ++i) {
        out.total += out.by_class[i];
        if (out.by_class[i] > 0) ++out.classes_used;
    }
    return out;
}

PwGrade pw_grade(const GameStats& s)
{
    PwGrade out;
    const PwProfile p = pw_profile(s);
    if (p.total <= 0 || !s.pw_codename_result_ok) {
        return out;
    }
    const bool all_weapons = p.dominant == WeaponClass::All;
    const bool coop = s.pw_camaraderie > 10000;
    const double ratio = pw_coop_ratio(s);
    const int heroism = s.pw_heroism;

    // Highest grade whose every gate passes; grades never decrease natively,
    // but this is the candidate the evaluator would compute right now.
    for (int grade = 1; grade <= 5; ++grade) {
        const int i = grade - 1;
        // Grade 3 and up need the ratio to reach 1.0, so it is >= not >.
        // Confirmed in the evaluator: jb against the 1.0 constant, jbe
        // against 0.5 and 0.05.
        const bool ratio_ok = grade >= 3 ? ratio >= kPwCoopRatioGate[i]
                                         : ratio > kPwCoopRatioGate[i];
        const bool cam_ok = coop ? s.pw_camaraderie > kPwCamaraderieStep[i]
                                 : s.pw_camaraderie <= kPwCamaraderieStep[i];
        const bool flag_ok = grade == 5 ? s.pw_codename_grade5_ok
            : grade == 4 ? s.pw_codename_grade4_ok : true;
        // Strictly greater: the evaluator skips the grade on jle against the
        // floor, so exactly 10000 Heroism does not earn grade 1.
        const bool heroism_ok = !all_weapons || heroism > kPwHeroismFloor[i];
        if (ratio_ok && cam_ok && flag_ok && heroism_ok) {
            out.grade = grade;
            continue;
        }
        // First failing grade is the one to report a blocker for.
        if (out.next == 0) {
            out.next = grade;
            if (!ratio_ok) {
                out.blocker = "cooperation ratio";
                out.have = ratio;
                out.need = kPwCoopRatioGate[i];
            } else if (!heroism_ok) {
                out.blocker = "heroism";
                out.have = heroism;
                out.need = kPwHeroismFloor[i];
            } else if (!cam_ok) {
                out.blocker = coop ? "camaraderie over" : "camaraderie under";
                out.have = s.pw_camaraderie;
                out.need = kPwCamaraderieStep[i];
            } else {
                out.blocker = grade == 5 ? "mission ranks (grade 5 flag)"
                                         : "mission ranks (grade 4 flag)";
                out.have = 0;
                out.need = 1;
            }
        }
    }
    return out;
}

std::optional<Match> evaluate_mgspw(const GameStats& s)
{
    const PwProfile p = pw_profile(s);
    if (p.total <= 0) {
        return std::nullopt;
    }
    const bool coop = s.pw_camaraderie > 10000;
    const bool nonlethal = p.nonlethal > 2 * p.lethal;
    for (const PwTitle& t : kPwTitles) {
        if (t.cls == p.dominant && t.coop == coop && t.nonlethal == nonlethal) {
            return Match{t.name, t.cls == WeaponClass::All ? Kind::Elite : Kind::Regular};
        }
    }
    return std::nullopt;
}

std::vector<ReqStatus> elite_requirements_mgspw(const GameStats& s)
{
    const PwProfile p = pw_profile(s);
    const auto row = [](const char* label, bool pass, double current, double limit,
                        Op op = Op::Ge) {
        return ReqStatus{label, pass, current, limit,
                         static_cast<uint8_t>(ReqFmt::Count),
                         static_cast<uint8_t>(op)};
    };
    std::vector<ReqStatus> out;
    // FOX is the solo all-weapons non-lethal title: spread the takedowns and
    // stay non-lethal. The two paths gate the spread differently, so each
    // reports the number its own gate is actually measured on.
    const PwAxes axes = pw_axes(s);
    if (axes.native) {
        // Every slot within a tenth of the average, the average taken over 11.
        int slot_total = 0;
        for (int value : axes.slot) slot_total += value;
        const double average = slot_total / 11.0;
        double worst = 0.0;
        if (average > 0.0) {
            for (int value : axes.slot) {
                const double off = 100.0 * std::abs(value - average) / average;
                if (off > worst) worst = off;
            }
        }
        out.push_back(row("slot spread %", slot_total > 0 && worst <= 10.0,
                          worst, 10.0, Op::Le));
    } else {
        out.push_back(row("classes used", p.classes_used >= kPwSpreadClasses,
                          p.classes_used, kPwSpreadClasses));
        const double top_share = p.total > 0
            ? 100.0 * static_cast<double>(p.top) / static_cast<double>(p.total)
            : 0.0;
        out.push_back(row("top class %", p.total > 0 && top_share < 100.0 * kPwSpreadShare,
                          top_share, 100.0 * kPwSpreadShare, Op::Lt));
    }
    // Shown against kills, the value it actually has to beat.
    out.push_back(row("non-lethal", p.nonlethal > 2 * p.lethal,
                      p.nonlethal, 2 * p.lethal));
    return out;
}

} // namespace bb::codename
