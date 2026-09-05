#include "../asi_main.h"
#include "../../overlay/overlay.h"
#include "probe.h"

void bb::asi_main()
{
    start_overlay(BB_GAME_LABEL, &mgspw::poll_stats,
                  L"METAL GEAR SOLID PEACE WALKER.exe", Game::MGSPW,
                  &mgspw::poll_stage_clock);
}
