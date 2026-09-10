#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>

namespace bb::mem {

inline bool copy(uintptr_t address, void* out, size_t size)
{
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), out,
                             size, &copied)
        && copied == size;
}

template <typename T>
bool copy(uintptr_t address, T& out)
{
    return copy(address, &out, sizeof(out));
}

inline bool module_timestamp(HMODULE module, uint32_t& out)
{
    const uintptr_t base = reinterpret_cast<uintptr_t>(module);
    IMAGE_DOS_HEADER dos{};
    if (!base || !copy(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE
        || dos.e_lfanew < 0 || dos.e_lfanew > 0x1000000) {
        return false;
    }
    IMAGE_NT_HEADERS nt{};
    if (!copy(base + static_cast<uintptr_t>(dos.e_lfanew), nt)
        || nt.Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }
    out = nt.FileHeader.TimeDateStamp;
    return true;
}

// Calls fn(begin, end) for each committed span of a module's code section,
// clamped to the section, and reports whether any call returned true. Both
// probes that hunt for a byte signature walk memory this way.
template <typename Fn>
bool for_each_code_region(HMODULE mod, Fn fn)
{
    const auto base = reinterpret_cast<uintptr_t>(mod);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(mod);
    if (!dos || dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }
    const uintptr_t start = base + nt->OptionalHeader.BaseOfCode;
    const uintptr_t end = start + nt->OptionalHeader.SizeOfCode;

    for (uintptr_t addr = start; addr < end; addr += 0x1000) {
        MEMORY_BASIC_INFORMATION mbi{};
        constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY
            | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))
            || mbi.State != MEM_COMMIT || (mbi.Protect & kReadable) == 0
            || (mbi.Protect & PAGE_GUARD) != 0) {
            continue;
        }
        const uintptr_t region_end =
            reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        if (fn(addr, region_end < end ? region_end : end)) {
            return true;
        }
        // Skip the rest of this region rather than re-querying every page.
        if (region_end > addr + 0x1000) {
            addr = region_end - 0x1000;
        }
    }
    return false;
}

template <typename T>
T read_at(uintptr_t base, size_t offset)
{
    T v{};
    std::memcpy(&v, reinterpret_cast<const uint8_t*>(base) + offset, sizeof(T));
    return v;
}

template <typename T>
T read(const uint8_t* data, size_t offset)
{
    T value{};
    std::memcpy(&value, data + offset, sizeof(value));
    return value;
}

template <typename T>
T read(uintptr_t address)
{
    T value{};
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
    return value;
}

} // namespace bb::mem
