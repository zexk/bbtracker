#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "check.h"
#include "common/format.h"
#include "games/mgspw/stat_reader.h"

using namespace bb;

namespace {

void test_formatting()
{
    char text[32];
    format_count(1234567, text, sizeof(text));
    CHECK(std::strcmp(text, "1,234,567") == 0);
    format_count(-1234, text, sizeof(text));
    CHECK(std::strcmp(text, "-1,234") == 0);
    format_time(3661, text, sizeof(text));
    CHECK(std::strcmp(text, "1:01:01") == 0);
}

void test_mission_stat()
{
    std::array<uint8_t, 0x28> record{};
    uint32_t flags = 0x40u << 16;
    int32_t value = 3;
    std::memcpy(record.data() + 0x10, &flags, sizeof(flags));
    std::memcpy(record.data() + 0x18, &value, sizeof(value));
    auto copy = [](uintptr_t address, void* out, size_t size) {
        std::memcpy(out, reinterpret_cast<const void*>(address), size);
        return true;
    };
    CHECK(mgspw::decode_mission_stat(record.data(), -1, copy) == 3);

    int32_t tallies[9] = {1, 0, 0, 0, 7};
    flags = 0;
    const uintptr_t pointer = reinterpret_cast<uintptr_t>(tallies);
    std::memcpy(record.data() + 0x10, &flags, sizeof(flags));
    std::memcpy(record.data() + 0x18, &pointer, sizeof(pointer));
    CHECK(mgspw::decode_mission_stat(record.data(), 4, copy) == 7);
    CHECK(mgspw::decode_mission_stat(record.data(), -1, copy) == -1);
    CHECK(mgspw::decode_mission_stat(record.data(), 9, copy) == -1);
    tallies[4] = -1;
    CHECK(mgspw::decode_mission_stat(record.data(), 4, copy) == -1);
}

} // namespace

int main()
{
    constexpr bb::test::Case tests[] = {
        {"formatting", test_formatting},
        {"mission_stat", test_mission_stat},
    };
    return bb::test::run("pw_portable", tests);
}
