#pragma once

#include "../../common/stats.h"

namespace bb::mgspw {

bool poll_stats(GameStats& out);

// The stage clock alone, in 300 Hz ticks. The full poll runs at 10 Hz, which
// a millisecond readout makes obvious; this is two loads, cheap enough to
// call once per drawn frame. False until poll_stats has resolved the anchors.
bool poll_stage_clock(uint32_t& ticks);

} // namespace bb::mgspw
