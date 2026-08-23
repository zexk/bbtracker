#include <windows.h>

#include "../../overlay/overlay.h"

DWORD WINAPI init_thread(LPVOID)
{
    bb::start_overlay(BB_GAME_LABEL, nullptr, L"METAL GEAR SOLID2.exe");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        HANDLE thread = CreateThread(nullptr, 0, init_thread, nullptr, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
