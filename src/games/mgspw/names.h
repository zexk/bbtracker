#pragma once

#include <iterator>
#include <string_view>

namespace bb::mgspw {

// English st_regionNNNN labels from SLOT 00001/12. Missing entries are placeholders.
inline constexpr const char* kRegionNames[] = {
    nullptr, // 0
    "MSF Base", // 1
    "Playa del Alba", // 2
    "Bosque del Alba", // 3
    "Puerto del Alba", // 4
    "El Cenagal: Jungle", // 5
    "El Cenagal: Ravine", // 6
    "El Cenagal: Swamp", // 7
    "Río del Jade", // 8
    "Bananal Fruta de Oro: Sorting Shed", // 9
    "Bananal Fruta de Oro: Farm", // 10
    "Camino de Lava: Hillside", // 11
    "Camino de Lava: Junction", // 12
    "Cafetal Aroma Encantado: Mill", // 13
    "Aldea Los Despiertos", // 14
    "El Cadalso", // 15
    "Los Cantos: Canyon", // 16
    "Los Cantos: Ridge", // 17
    "Fuerte La Ladera", // 18
    "Crater Base", // 19
    "Selva de la Leche: Jungle", // 20
    "Selva de la Leche: Hillside", // 21
    "Catarata de la Muerte", // 22
    "Selva de la Muerte: Bottom of Cliff", // 23
    "Selva de la Muerte: Top of Cliff", // 24
    "Ruinas de Xochiquetzal", // 25
    "AI Laboratory", // 26
    "Miners' Residence", // 27
    "Mining Pit", // 28
    "AI Weapon Hangar", // 29
    "Underground Passage A", // 30
    "Underground Passage B", // 31
    "Torture Chamber", // 32
    "Back Gate", // 33
    "Small Maintenance Dock", // 34
    "Rooftop", // 35
    "Main Maintenance Dock", // 36
    "Runway", // 37
    "Kill House", // 38
    "Deck", // 39
    "Isla del Monstruo", // 40
    nullptr, // 41
    nullptr, // 42
    nullptr, // 43
    nullptr, // 44
    nullptr, // 45
    nullptr, // 46
    nullptr, // 47
    nullptr, // 48
    nullptr, // 49
    "Cafetal Aroma Encantado: Entrance", // 50
    "Underpass", // 51
    "Shooting Range", // 52
    "Heliport", // 53
};

constexpr const char* region_name(int id) {
    return id >= 0 && static_cast<std::size_t>(id) < std::size(kRegionNames)
        ? kRegionNames[id] : nullptr;
}

constexpr int region_id(int index) {
    return index == 70 ? 7 : (index >= 0 && index < 53 ? index + 1 : -1);
}

static_assert(region_id(3) == 4 && region_id(4) == 5 && region_id(70) == 7);
static_assert(region_id(-1) == -1 && region_id(0x7fffffff) == -1);
static_assert(std::string_view(region_name(region_id(3))) == "Puerto del Alba");
static_assert(std::string_view(region_name(region_id(4))) == "El Cenagal: Jungle");
static_assert(region_name(-1) == nullptr && region_name(54) == nullptr);
static_assert(region_name(0) == nullptr && region_name(41) == nullptr);

} // namespace bb::mgspw
