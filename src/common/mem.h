#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>

namespace bb::mem {

inline bool range_readable(uintptr_t addr, size_t len)
{
    const uintptr_t end = addr + len;
    while (addr < end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) {
            return false;
        }
        if (mbi.State != MEM_COMMIT) {
            return false;
        }
        constexpr DWORD kReadable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY
            | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if ((mbi.Protect & kReadable) == 0 || (mbi.Protect & PAGE_GUARD) != 0) {
            return false;
        }
        const uintptr_t region_end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        addr = region_end;
    }
    return true;
}

inline bool readable(uintptr_t address, size_t size)
{
    MEMORY_BASIC_INFORMATION memory{};
    if (!VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory))) return false;
    constexpr DWORD access = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY
        | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return memory.State == MEM_COMMIT && (memory.Protect & access) != 0
        && (memory.Protect & PAGE_GUARD) == 0
        && address + size <= reinterpret_cast<uintptr_t>(memory.BaseAddress) + memory.RegionSize;
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