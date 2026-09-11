#pragma once

#include <cstdlib>
#include <cstring>
#include <iterator>

#include "../../common/area.h"

namespace bb::mgs3 {

inline constexpr const char* kCaptures[] = {
    "King Cobra", "Taiwanese Cobra", "Thai Cobra", "Coral Snake",
    "Milk Snake", "Green Tree Python", "Giant Anaconda", "Reticulated Python",
    "Snake Liquid", "Snake Solid", "Snake Solidus", "Indian Gavial",
    "Otton Frog", "Tree Frog", "Poison Dart Frog", "Rat",
    "European Rabbit", "Flying Squirrel", "Markhor", "Vampire Bat",
    "Hornet's Nest", "Emperor Scorpion", "Cobalt Blue Tarantula", "Parrot",
    "White-Rumped Vulture", "Red Avadavat", "Magpie", "Sund Whistling-Thrush",
    "Bigeye Trevally", "Maroon Shark", "Arowana", "Kenyan Mangrove Crab",
    "Russian Oyster Mushroom", "Ural Luminescent Mushroom", "Siberian Ink Cap",
    "Fly Agaric", "Russian Glowcap", "Spatsa", "Baikal Scaly Tooth",
    "Yabloko Moloko", "Russian False Mango", "Golova", "Vine Melon",
    "Instant Noodles", "Russian Ration", "Calorie Mate", "Hive of Pain Hornets",
    "Tsuchinoko",
};

inline constexpr const char* kKerotans[] = {
    "01 Dremuchij South (VM)", "02 Dremuchij Swampland (VM)", "03 Dremuchij North (VM)",
    "04 Dolinovodno (VM)", "05 Rassvet (VM)", "06 Dremuchij South",
    "07 Dremuchij East", "08 Dremuchij Swampland", "09 Dremuchij North",
    "10 Dolinovodno", "11 Rassvet", "12 Chyornyj Prud",
    "13 Bolshaya Past South", "14 Bolshaya Past Base", "15 Bolshaya Past Crevice",
    "16 Chyornaya Peschera Cave Branch", "17 Chyornaya Peschera Cave", "18 Chyornaya Peschera Cave Entrance",
    "19 Ponizovje South", "20 Ponizovje West", "21 Ponizovje Warehouse Exterior",
    "22 Ponizovje Warehouse 1F", "23 Svyatogornyj South", "24 Graniny Gorki South",
    "25 Graniny Gorki Lab Exterior Perimeter", "26 Graniny Gorki Lab Exterior Yard", "27 Graniny Gorki Lab 1F",
    "28 Graniny Gorki Lab B1 East", "29 Graniny Gorki Lab B1 West", "30 Svyatogornyj West",
    "31 Svyatogornyj East", "32 Sokrovenno South", "33 Sokrovenno West",
    "34 Sokrovenno North", "35 Krasnogorje Tunnel", "36 Krasnogorje Mountain Base",
    "37 Krasnogorje Mountainside", "38 Krasnogorje Mountaintop", "39 Krasnogorje Mountaintop Ruins",
    "40 Krasnogorje Mountaintop Behind Ruins", "41 Groznyj Grad Underground Tunnel", "42 Groznyj Grad Southwest",
    "43 Groznyj Grad Northwest", "44 Groznyj Grad Northeast", "45 Groznyj Grad Southeast",
    "46 Weapons Lab East Wing 2F", "47 Weapons Lab West Wing 2F Corridor", "48 Groznyj Grad Holding Facility",
    "49 Weapons Lab Main Wing 1F", "50 Groznyj Grad B1F", "51 Tikhogornyj",
    "52 Tikhogornyj Behind Waterfall", "53 Groznyj Grad Escape", "54 Groznyj Grad Runway South",
    "55 Groznyj Grad Runway", "56 Groznyj Grad Runway After WIG", "57 Rail Bridge C3",
    "58 Rail Bridge Shagohod", "59 Rail Bridge North", "60 Lazorevo South",
    "61 Lazorevo North", "62 Zaozyorje West", "63 Zaozyorje East",
    "64 Rokovoj Bereg",
};

static_assert(std::size(kCaptures) == 48);
static_assert(std::size(kKerotans) == 64);

inline int area_kerotan(const char* code)
{
    static constexpr AreaName kAreas[] = {
        {"v001a", "0"},
        {"v003a", "1"},
        {"v004a", "2"},
        {"v005a", "3"},
        {"v006a", "4"},
        {"v006b", "4"},
        {"s001a", "5"},
        {"s002a", "6"},
        {"s003a", "7"},
        {"s004a", "8"},
        {"s005a", "9"},
        {"s006a", "10"},
        {"s006b", "10"},
        {"s012a", "11"},
        {"s021a", "12"},
        {"s022a", "13"},
        {"s023a", "14"},
        {"s031a", "15"},
        {"s032a", "16"},
        {"s032b", "16"},
        {"s033a", "17"},
        {"s041a", "18"},
        {"s042a", "19"},
        {"s043a", "20"},
        {"s044a", "21"},
        {"s045a", "22"},
        {"s051a", "23"},
        {"s051b", "23"},
        {"s052a", "24"},
        {"s052b", "25"},
        {"s053a", "26"},
        {"s055a", "27"},
        {"s056a", "28"},
        {"s061a", "29"},
        {"s062a", "30"},
        {"s063a", "31"},
        {"s063b", "31"},
        {"s064a", "32"},
        {"s064b", "32"},
        {"s065a", "33"},
        {"s065b", "33"},
        {"s066a", "34"},
        {"s071a", "35"},
        {"s072a", "36"},
        {"s072b", "36"},
        {"s073a", "37"},
        {"s073b", "37"},
        {"s074a", "38"},
        {"s075a", "39"},
        {"s081a", "40"},
        {"s091a", "41"},
        {"s091b", "41"},
        {"s091c", "41"},
        {"s092a", "42"},
        {"s092b", "42"},
        {"s092c", "42"},
        {"s093a", "43"},
        {"s093b", "43"},
        {"s093c", "43"},
        {"s094a", "44"},
        {"s094b", "44"},
        {"s094c", "44"},
        {"s101a", "45"},
        {"s101b", "45"},
        {"s111a", "46"},
        {"s112a", "47"},
        {"s121a", "48"},
        {"s122a", "49"},
        {"s151a", "50"},
        {"s152a", "51"},
        {"s161a", "52"},
        {"s162a", "53"},
        {"s163a", "54"},
        {"s163b", "55"},
        {"s171a", "56"},
        {"s171b", "57"},
        {"s181a", "58"},
        {"s182a", "59"},
        {"s183a", "60"},
        {"s191a", "61"},
        {"s191b", "61"},
        {"s192a", "62"},
        {"s201a", "63"},
    };
    for (const AreaName& area : kAreas) {
        if (std::strncmp(code, area.code, 5) == 0
            && (code[5] == '\0' || code[5] == '_')) {
            return std::atoi(area.name);
        }
    }
    return -1;
}

} // namespace bb::mgs3
