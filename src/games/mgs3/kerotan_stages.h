#pragma once

#include <cstdint>

namespace bb::mgs3 {

struct KerotanStage {
    const char* code;
    const char* name;
};

// 64 Kerotan frogs, one per area (some areas have none, a few have multiples
// collapsed into one entry here). Order matches in-game Kerotan counter.
constexpr KerotanStage kKerotanStages[] = {
    {"v001a", "Dremuchij South"},
    {"v003a", "Dremuchij Swampland"},
    {"v004a", "Dremuchij North"},
    {"v005a", "Dolinovodno"},
    {"v006a", "Rassvet"},
    {"s001a", "Dremuchij South"},
    {"s002a", "Dremuchij East"},
    {"s003a", "Dremuchij Swampland"},
    {"s004a", "Dremuchij North"},
    {"s005a", "Dolinovodno"},
    {"s006a", "Rassvet"},
    {"s012a", "Chyornyj Prud"},
    {"s021a", "Bolshaya Past South"},
    {"s022a", "Bolshaya Past Base"},
    {"s031a", "Chyornaya Peschera Branch"},
    {"s032a", "Chyornaya Peschera Cave"},
    {"s033a", "Chyornaya Peschera Entrance"},
    {"s041a", "Ponizovje South"},
    {"s042a", "Ponizovje West"},
    {"s043a", "Ponizovje Warehouse Ext."},
    {"s044a", "Ponizovje Warehouse"},
    {"s045a", "Svyatogornyj South"},
    {"s051a", "Graniny Gorki South"},
    {"s052a", "Gorki Lab Exterior"},
    {"s053a", "Graniny Gorki Lab 1F"},
    {"s055a", "Gorki Lab B1 East"},
    {"s056a", "Gorki Lab B1 West"},
    {"s061a", "Svyatogornyj West"},
    {"s062a", "Svyatogornyj East"},
    {"s063a", "Sokrovenno South"},
    {"s064a", "Sokrovenno West"},
    {"s065a", "Sokrovenno North"},
    {"s066a", "Krasnogorje Tunnel"},
    {"s071a", "Krasnogorje Mtn Base"},
    {"s072a", "Krasnogorje Mountainside"},
    {"s073a", "Krasnogorje Mountaintop"},
    {"s074a", "Krasnogorje Ruins"},
    {"s075a", "Krasnogorje Behind Ruins"},
    {"s081a", "Groznyj Grad Tunnel"},
    {"s091a", "Groznyj Grad SW"},
    {"s092a", "Groznyj Grad"},
    {"s093a", "Groznyj Grad SE"},
    {"s094a", "Groznyj Grad Weapons Lab"},
    {"s095a", "Shagohod Hangar"},
    {"s0a0a", "Zaozyorje West"},
    {"s0b0a", "Zaozyorje East"},
    {"s0c0a", "Rokovoj Bereg"},
};

constexpr int kKerotanCount = sizeof(kKerotanStages) / sizeof(kKerotanStages[0]);
static_assert(kKerotanCount <= 64, "kerotan_mask is uint64_t");

} // namespace bb::mgs3
