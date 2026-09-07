#include "../asi_main.h"
#include "../../overlay/overlay.h"
#include "probe.h"

void bb::asi_main()
{
    start_overlay(BB_GAME_LABEL, &babel::poll_stats,
                  L"MGS MC2 Bonus Content.exe", Game::Babel);
}
