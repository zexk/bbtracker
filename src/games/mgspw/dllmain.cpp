#include <windows.h>

#include "../../overlay/overlay.h"
#include "probe.h"

DWORD WINAPI init_thread(LPVOID)
{
    bb::start_overlay("METAL GEAR SOLID: PEACE WALKER", &bb::mgspw::poll_stats,
                      L"METAL GEAR SOLID PEACE WALKER.exe", bb::Game::MGSPW);
    return 0;
}

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
