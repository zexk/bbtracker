#include <windows.h>

#include "../../overlay/overlay.h"
#include "probe.h"

DWORD WINAPI init_thread(LPVOID)
{
    while (!GetModuleHandleW(L"mg1.dll") && !GetModuleHandleW(L"mg2.dll")) Sleep(100);
    if (GetModuleHandleW(L"mg1.dll")) {
        bb::start_overlay("METAL GEAR", &bb::mg12::poll_mg1, L"METAL GEAR.exe", bb::Game::MG1);
    } else {
        bb::start_overlay("METAL GEAR 2", &bb::mg12::poll_mg2, L"METAL GEAR.exe", bb::Game::MG2);
    }
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
