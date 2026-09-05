#include "../asi_main.h"
#include "../../overlay/overlay.h"
#include "probe.h"

void bb::asi_main()
{
    start_overlay(BB_GAME_LABEL, &mgs3::poll_stats, L"METAL GEAR SOLID3.exe", Game::MGS3);
}
