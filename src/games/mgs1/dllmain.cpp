#include <windows.h>

#include "../../overlay/overlay.h"
#include "probe.h"

DWORD WINAPI init_thread(LPVOID)
{
    bb::start_overlay(BB_GAME_LABEL, &bb::mgs1::poll_stats, L"METAL GEAR SOLID.exe", bb::Game::MGS1);
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

extern "C" __declspec(dllexport) void __stdcall bbtracker_rundll(HWND, HINSTANCE, LPSTR, int)
{
    MessageBoxA(nullptr, "bbtracker loaded and DllMain ran", "bbtracker", MB_OK);
}
