#include "probe.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "../../common/log.h"
#include "../../common/mem.h"

namespace bb::babel {

using bb::mem::range_readable;
using bb::mem::read;

namespace {

constexpr char kRomTitle[] = "METALGEARGB";
constexpr uintptr_t kRomTitleOffset = 0x134;
constexpr uintptr_t kRecordPointerOffset = 0x24;
constexpr uintptr_t kRecordRomOffset = 0x58;
constexpr uintptr_t kRecordWram0Offset = 0xB8;
constexpr uintptr_t kRecordSelectedWramOffset = 0xD0;
constexpr uintptr_t kWramBankSize = 0x1000;

uintptr_t g_record = 0;
uintptr_t g_scan_cursor = 0x10000;

bool resolve_record(const char* title)
{
    const uintptr_t rom = reinterpret_cast<uintptr_t>(title) - kRomTitleOffset;
    if (rom < kRecordPointerOffset
        || !range_readable(rom - kRecordPointerOffset, sizeof(uintptr_t))) {
        return false;
    }
    const uintptr_t record = read<uintptr_t>(rom - kRecordPointerOffset);
    if (!record || !range_readable(record + kRecordRomOffset, sizeof(uintptr_t))
        || read<uintptr_t>(record + kRecordRomOffset) != rom
        || !range_readable(record + kRecordWram0Offset, sizeof(uintptr_t))
        || !range_readable(record + kRecordSelectedWramOffset, sizeof(uintptr_t))) {
        return false;
    }
    const uintptr_t wram0 = read<uintptr_t>(record + kRecordWram0Offset);
    const uintptr_t selected = read<uintptr_t>(record + kRecordSelectedWramOffset);
    if (!range_readable(wram0, 8 * kWramBankSize)
        || selected < wram0 + kWramBankSize || selected >= wram0 + 8 * kWramBankSize
        || (selected - wram0) % kWramBankSize != 0) {
        return false;
    }
    g_record = record;
    g_scan_cursor = 0x10000;
    LOG_INFO("babel memory map %p, WRAM %p, selected bank %p",
             reinterpret_cast<void*>(record), reinterpret_cast<void*>(wram0),
             reinterpret_cast<void*>(selected));
    return true;
}

void scan_record()
{
    LARGE_INTEGER frequency{};
    LARGE_INTEGER started{};
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&started);
    const int64_t deadline = started.QuadPart + frequency.QuadPart / 500;

    MEMORY_BASIC_INFORMATION mbi{};
    while (VirtualQuery(reinterpret_cast<LPCVOID>(g_scan_cursor), &mbi, sizeof(mbi))) {
        const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        const uintptr_t end = base + mbi.RegionSize;
        constexpr DWORD kWritable = PAGE_READWRITE | PAGE_WRITECOPY
            | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if (mbi.State == MEM_COMMIT && (mbi.Protect & kWritable) != 0
            && (mbi.Protect & PAGE_GUARD) == 0 && mbi.RegionSize <= 0x10000000) {
            const char* next = reinterpret_cast<const char*>(
                g_scan_cursor > base ? g_scan_cursor : base);
            const char* limit = reinterpret_cast<const char*>(end);
            while (next < limit) {
                const char* hit = static_cast<const char*>(
                    std::memchr(next, 'M', static_cast<size_t>(limit - next)));
                if (!hit) break;
                next = hit + 1;
                if (static_cast<size_t>(limit - hit) >= sizeof(kRomTitle) - 1
                    && std::memcmp(hit, kRomTitle, sizeof(kRomTitle) - 1) == 0
                    && resolve_record(hit)) {
                    return;
                }
                LARGE_INTEGER now{};
                QueryPerformanceCounter(&now);
                if (now.QuadPart >= deadline) {
                    g_scan_cursor = reinterpret_cast<uintptr_t>(next);
                    return;
                }
            }
        }
        if (end <= g_scan_cursor) break;
        g_scan_cursor = end;
    }
    g_scan_cursor = 0x10000;
}

uint16_t value16(uintptr_t wram0, uintptr_t address)
{
    return read<uint16_t>(wram0 + address - 0xC000);
}

} // namespace

bool poll_stats(GameStats& out)
{
    if (!g_record) {
        scan_record();
        if (!g_record) return false;
    }
    if (!range_readable(g_record + kRecordSelectedWramOffset, sizeof(uintptr_t))) {
        g_record = 0;
        return false;
    }
    const uintptr_t wram0 = read<uintptr_t>(g_record + kRecordWram0Offset);
    const uintptr_t bank6 = wram0 + 6 * kWramBankSize;
    if (!range_readable(wram0, 8 * kWramBankSize)) {
        g_record = 0;
        return false;
    }

    const uint8_t difficulty = read<uint8_t>(wram0 + 0x4E7);
    const uint8_t stage = read<uint8_t>(wram0 + 0x46C);
    const uint8_t mode = read<uint8_t>(wram0 + 0x0F3);
    if (difficulty > 3 || stage > 12 || (mode & 0x0F) != 0) return false;

    const uint8_t frames = read<uint8_t>(wram0 + 0x4F8);
    const uint8_t seconds = read<uint8_t>(wram0 + 0x4F9);
    const uint8_t minutes = read<uint8_t>(wram0 + 0x4FA);
    const uint8_t hours = read<uint8_t>(wram0 + 0x4FB);
    const uint8_t career_seconds = read<uint8_t>(bank6 + 0xF55);
    const uint8_t career_minutes = read<uint8_t>(bank6 + 0xF56);
    const uint8_t career_hours = read<uint8_t>(bank6 + 0xF57);
    if (frames >= 60 || seconds >= 60 || minutes >= 60 || hours >= 100
        || career_seconds >= 60 || career_minutes >= 60 || career_hours >= 100) {
        return false;
    }

    out = {};
    out.difficulty_raw = difficulty;
    out.difficulty_game_byte = difficulty;
    out.difficulty = difficulty == 0 ? Difficulty::Easy
        : difficulty == 1 ? Difficulty::Normal
        : difficulty == 2 ? Difficulty::Hard : Difficulty::Extreme;
    out.mission = stage;
    out.alerts = value16(wram0, 0xC4EE) + read<uint16_t>(bank6 + 0xF4A);
    out.kills = value16(wram0, 0xC4F0) + read<uint16_t>(bank6 + 0xF4C);
    out.rations_used = value16(wram0, 0xC4F2) + read<uint16_t>(bank6 + 0xF4E);
    out.saves = read<uint8_t>(wram0 + 0x44A);
    out.play_time_seconds = career_seconds + career_minutes * 60.0
        + career_hours * 3600.0 + seconds + minutes * 60.0 + hours * 3600.0
        + frames / 60.0;
    return out.play_time_seconds > 0.0;
}

} // namespace bb::babel
