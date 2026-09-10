#include "probe.h"

#include <windows.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "../../common/log.h"
#include "../../common/mem.h"
#include "../../common/run_latch.h"
#include "../../overlay/overlay.h"
#include "../asi_main.h"

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
    static std::vector<uint8_t> buffer(kScanChunkSize + kTitleSize - 1);
    mem::scan(g_scan_cursor, buffer, kScanChunkSize, kTitleSize - 1,
              mem::kWritable, 0, 0x10000000, 500,
              [&](uintptr_t address, const uint8_t* data, size_t primary, size_t size) {
        const auto* next = data;
        const auto* const limit = data + primary;
        while (next < limit) {
            const auto* hit = static_cast<const uint8_t*>(
                std::memchr(next, 'M', static_cast<size_t>(limit - next)));
            if (!hit) break;
            next = hit + 1;
            if (static_cast<size_t>(data + size - hit) >= kTitleSize
                && std::memcmp(hit, kRomTitle, kTitleSize) == 0
                && resolve_record(address + static_cast<uintptr_t>(hit - data))) return true;
        }
        return false;
    });
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

void bb::asi_main()
{
    start_overlay(BB_GAME_LABEL, &babel::poll_stats,
                  L"MGS MC2 Bonus Content.exe", Game::Babel);
}
