// The DLL entry point every game's ASI shares. Only asi_main() differs, so
// the Win32 side of attaching lives here rather than in six copies.

#include <windows.h>

#include "asi_main.h"

namespace {

DWORD WINAPI init_thread(LPVOID)
{
    bb::asi_main();
    return 0;
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        if (HANDLE thread = CreateThread(nullptr, 0, init_thread, nullptr, 0, nullptr)) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
