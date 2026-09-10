#pragma once

#include "stats.h"

namespace bb {

enum class RunState {
    Unknown,
    Active,
    Inactive,
};

class RunLatch {
public:
    constexpr bool hold(GameStats& out) const
    {
        if (visible_) out = last_;
        return visible_;
    }

    constexpr bool update(GameStats& out, RunState state)
    {
        if (state == RunState::Unknown) return hold(out);
        visible_ = state == RunState::Active;
        if (visible_) last_ = out;
        return visible_;
    }

private:
    bool visible_ = false;
    GameStats last_{};
};

constexpr bool run_latch_holds_last_complete_snapshot()
{
    RunLatch run;
    GameStats stats{};
    stats.kills = 7;
    run.update(stats, RunState::Active);
    stats.kills = 0;
    return run.update(stats, RunState::Unknown) && stats.kills == 7;
}

static_assert(run_latch_holds_last_complete_snapshot());

} // namespace bb
