#!/usr/bin/env python3
"""Compile the real PW stat readers against bounded, synthetic memory."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/games/mgspw/probe.cpp").read_text()


def block(start, end):
    return source[source.index(start):source.index(end)]


code = r'''
#include <cassert>
#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string_view>
#include <utility>
#include <vector>
#include "common/stats.h"
using namespace bb;
uintptr_t g_stat_array = 0, g_local_player = 0;
std::vector<std::pair<uintptr_t, size_t>> regions;
bool range_readable(uintptr_t address, size_t size) {
    for (auto [base, length] : regions)
        if (address >= base && address - base <= length
            && size <= length - (address - base)) return true;
    return false;
}
namespace bb::mem {
bool copy(uintptr_t address, void* out, size_t size) {
    if (!range_readable(address, size)) return false;
    std::memcpy(out, reinterpret_cast<const void*>(address), size);
    return true;
}
template<class T> bool copy(uintptr_t address, T& out) {
    return copy(address, &out, sizeof(out));
}
template<class T> T read(uintptr_t address) {
    T value{};
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
    return value;
}
}
template<class T> void put(uintptr_t address, T value) {
    std::memcpy(reinterpret_cast<void*>(address), &value, sizeof(value));
}
'''
code += block("constexpr uint32_t kStatMax", "// save+0x1BFF0")
code += block("void read_codename_axes", "bool pat_match")
code += r'''
int main() {
    alignas(8) uint8_t records[kStatRecordsSpan]{};
    const auto base = reinterpret_cast<uintptr_t>(records);
    uintptr_t array = base + kStatArrayBias;
    uint8_t player = 4;
    int32_t tallies[9] = {1, 0, 0, 0, 7};
    g_stat_array = reinterpret_cast<uintptr_t>(&array);
    g_local_player = reinterpret_cast<uintptr_t>(&player);
    const auto values = reinterpret_cast<uintptr_t>(tallies);
    regions = {{base, sizeof(records)}, {g_stat_array, sizeof(array)},
               {g_local_player, 1}, {values, sizeof(tallies)}};
    for (uint32_t index = 0; index <= kStatIndexMax; ++index)
        put(base + index * kStatStride + 0x10, 0x20000u | index);
    const uintptr_t rod = base + 0x105 * kStatStride;
    put(rod + 0x18, values);
    put(rod + 0x20, int32_t{12});
    assert(mission_stat(rod, 0) == 1);
    assert(mission_stat(rod, 4) == 7);
    assert(mission_stat(rod, -1) == -1);
    assert(mission_stat(rod, 9) == -1);
    put(rod + 0x18, uintptr_t{1}); // A small pointer is not a scalar count.
    assert(mission_stat(rod, 0) == -1);
    put(rod + 0x18, values);
    tallies[4] = -2;
    assert(mission_stat(rod, 4) == -1);
    tallies[4] = 1000000;
    assert(mission_stat(rod, 4) == -1);
    tallies[4] = 7;
    const uintptr_t scalar = base + 0x31 * kStatStride;
    put(scalar + 0x10, uint32_t{0x4420031});
    put(scalar + 0x18, int32_t{3});
    assert(mission_stat(scalar, -1) == 3);
    put(scalar + 0x10, uint32_t{0x2620031});
    assert(mission_stat(scalar, 0) == -1);
    put(scalar + 0x10, uint32_t{0x4420031});
    GameStats stats;
    read_stat_families(0, stats);
    assert(stats.pw_stun_rod_takedowns == 12);
    assert(stats.pw_m_stun_rod_takedowns == 7);
    assert(stats.pw_m_headshots == 3);
    player = 0;
    read_stat_families(0, stats);
    assert(stats.pw_m_stun_rod_takedowns == 1);
    g_local_player = 0;
    read_stat_families(0, stats);
    assert(stats.pw_m_stun_rod_takedowns == -1);
    assert(stats.pw_m_headshots == 3);
    read_codename_axes(stats);
    assert(stats.pw_codename_axes_ok && stats.pw_codename_axes[3][1] == 12);
    put(rod + 0x10, uint32_t{0x20106});
    read_codename_axes(stats);
    assert(!stats.pw_codename_axes_ok);
    put(rod + 0x10, uint32_t{0x20105});
    put(rod + 0x20, int32_t{-1});
    read_codename_axes(stats);
    assert(!stats.pw_codename_axes_ok);
    array = 0;
    read_stat_families(0, stats);
    assert(stats.pw_stun_rod_takedowns == -1);
    assert(stats.pw_m_stun_rod_takedowns == -1);
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / "test.cpp").write_text(code)
    subprocess.run(shlex.split(os.environ.get("CXX", "c++")) + [
        "-std=c++20", "-UNDEBUG", f"-I{root / 'src'}", str(path / "test.cpp"),
        "-o", str(path / "test"),
    ], check=True)
    subprocess.run([str(path / "test")], check=True)
print("PW probe checks passed")
