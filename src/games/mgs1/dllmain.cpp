#include "../asi_main.h"
#include "../../overlay/overlay.h"
#include "probe.h"

void bb::asi_main()
{
    start_overlay(BB_GAME_LABEL, &mgs1::poll_stats, L"METAL GEAR SOLID.exe", Game::MGS1);
}
