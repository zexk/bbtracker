#include "probe.h"

#include <windows.h>

#include "../../common/log.h"

namespace bb::mgs4 {
namespace {

constexpr wchar_t kModuleName[] = L"MGS4.exe";

} // namespace

bool poll_stats(GameStats& out)
{
    HMODULE mod = GetModuleHandleW(kModuleName);
    if (!mod) {
        return false;
    }
    return false;
}

} // namespace bb::mgs4
