#pragma once

#include <cstdint>
#include <cstring>

namespace bb::mgspw {

template <typename Copy>
int decode_mission_stat(const uint8_t* record, int player, Copy copy)
{
    uint32_t flags;
    std::memcpy(&flags, record + 0x10, sizeof(flags));
    flags >>= 16;
    if (flags & 0x220) return -1;

    int32_t value;
    if (flags & 0x40) {
        std::memcpy(&value, record + 0x18, sizeof(value));
    } else {
        uintptr_t pointer;
        std::memcpy(&pointer, record + 0x18, sizeof(pointer));
        if (player < 0 || player > 8 || !pointer
            || !copy(pointer + player * sizeof(value), &value, sizeof(value))) return -1;
    }
    return value >= 0 && value <= 999999 ? value : -1;
}

} // namespace bb::mgspw
