#include "probe.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "../../common/log.h"
#include "../../common/mem.h"
#include "../../common/run_latch.h"

namespace bb::babel {

using bb::mem::read;

namespace {

constexpr char kRomTitle[] = "METALGEARGB";
constexpr uintptr_t kRomTitleOffset = 0x134;
constexpr uintptr_t kRecordPointerOffset = 0x24;
constexpr uintptr_t kRecordRomOffset = 0x58;
constexpr uintptr_t kRecordWram0Offset = 0xB8;
constexpr uintptr_t kRecordSelectedWramOffset = 0xD0;
uintptr_t g_record = 0;
uintptr_t g_scan_cursor = 0x10000;
RunLatch g_run;

constexpr bool empty_wram_is_reset()
{
    std::array<uint8_t, kWramSize> wram{};
    return run_reset(wram.data());
}

static_assert(empty_wram_is_reset());

bool resolve_record(uintptr_t title)
{
    const uintptr_t rom = title - kRomTitleOffset;
    uintptr_t record = 0;
    if (rom < kRecordPointerOffset
        || !mem::copy(rom - kRecordPointerOffset, record) || !record) {
        return false;
    }
    std::array<uint8_t, kRecordSelectedWramOffset - kRecordRomOffset + sizeof(uintptr_t)> data{};
    if (!mem::copy(record + kRecordRomOffset, data.data(), data.size())) {
        return false;
    }
    const uintptr_t base = reinterpret_cast<uintptr_t>(data.data());
    const uintptr_t record_rom = read<uintptr_t>(base);
    const uintptr_t wram0 = read<uintptr_t>(base + kRecordWram0Offset - kRecordRomOffset);
    const uintptr_t selected = read<uintptr_t>(
        base + kRecordSelectedWramOffset - kRecordRomOffset);
    if (record_rom != rom || !wram0 || wram0 > UINTPTR_MAX - kWramSize
        || selected < wram0 + kWramBankSize || selected >= wram0 + kWramSize
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
    constexpr size_t kScanChunkSize = 0x40000;
    constexpr size_t kTitleSize = sizeof(kRomTitle) - 1;
    static std::vector<char> buffer(kScanChunkSize + kTitleSize - 1);
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
            uintptr_t address = g_scan_cursor > base ? g_scan_cursor : base;
            while (address < end) {
                const size_t primary = std::min(kScanChunkSize, end - address);
                const size_t read_size = std::min(primary + kTitleSize - 1, end - address);
                if (!mem::copy(address, buffer.data(), read_size)) break;
                const char* next = buffer.data();
                const char* const search_limit = next + primary;
                const char* const read_limit = buffer.data() + read_size;
                while (next < search_limit) {
                    const char* hit = static_cast<const char*>(
                        std::memchr(next, 'M', static_cast<size_t>(search_limit - next)));
                    if (!hit) break;
                    next = hit + 1;
                    if (static_cast<size_t>(read_limit - hit) >= kTitleSize
                        && std::memcmp(hit, kRomTitle, kTitleSize) == 0
                        && resolve_record(address + static_cast<uintptr_t>(hit - buffer.data()))) {
                        return;
                    }
                }
                LARGE_INTEGER now{};
                QueryPerformanceCounter(&now);
                if (now.QuadPart >= deadline) {
                    g_scan_cursor = address + primary;
                    return;
                }
                address += primary;
            }
        }
        if (end <= g_scan_cursor) break;
        g_scan_cursor = end;
    }
    g_scan_cursor = 0x10000;
}

} // namespace

bool poll_stats(GameStats& out)
{
    if (!g_record) {
        scan_record();
        if (!g_record) return g_run.hold(out);
    }
    std::array<uint8_t, kRecordSelectedWramOffset - kRecordWram0Offset
                            + sizeof(uintptr_t)> record{};
    if (!mem::copy(g_record + kRecordWram0Offset, record.data(), record.size())) {
        g_record = 0;
        return g_run.hold(out);
    }
    const uintptr_t record_data = reinterpret_cast<uintptr_t>(record.data());
    const uintptr_t wram0 = read<uintptr_t>(record_data);
    const uintptr_t selected = read<uintptr_t>(
        record_data + kRecordSelectedWramOffset - kRecordWram0Offset);
    if (!wram0 || wram0 > UINTPTR_MAX - kWramSize
        || selected < wram0 + kWramBankSize || selected >= wram0 + kWramSize
        || (selected - wram0) % kWramBankSize != 0) {
        g_record = 0;
        return g_run.hold(out);
    }
    std::array<uint8_t, kWramSize> wram{};
    if (!mem::copy(wram0, wram.data(), wram.size())) return g_run.hold(out);
    const uint8_t* const data = wram.data();
    if (run_reset(data)) return g_run.update(out, RunState::Inactive);
    if (!decode_wram(data, out)) return g_run.hold(out);
    return g_run.update(out, out.play_time_seconds > 0.0
                                 ? RunState::Active : RunState::Unknown);
}

} // namespace bb::babel
