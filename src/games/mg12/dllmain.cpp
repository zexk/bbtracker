#include <windows.h>

#include "../asi_main.h"
#include "../../overlay/overlay.h"
#include "probe.h"

void bb::asi_main()
{
    // One ASI covers both games, so wait to see which module loaded before
    // picking the label and the probe.
    while (!GetModuleHandleW(L"mg1.dll") && !GetModuleHandleW(L"mg2.dll")) Sleep(100);
    if (GetModuleHandleW(L"mg1.dll")) {
        start_overlay("METAL GEAR", &mg12::poll_mg1, L"METAL GEAR.exe", Game::MG1);
    } else {
        start_overlay("METAL GEAR 2", &mg12::poll_mg2, L"METAL GEAR.exe", Game::MG2);
    }
}
