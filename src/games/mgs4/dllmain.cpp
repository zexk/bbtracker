#include "../asi_main.h"
#include "../../overlay/overlay.h"
#include "probe.h"

void bb::asi_main()
{
    start_overlay(BB_GAME_LABEL, &mgs4::poll_stats, L"mgs4.exe", Game::MGS4);
}
