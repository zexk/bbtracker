#pragma once

#include "stats.h"

namespace bb {

enum class RunState {
    Unknown,
    Active,
    Inactive,
};

constexpr bool next_run_visibility(bool visible, RunState state)
{
    if (state == RunState::Active) return true;
    if (state == RunState::Inactive) return false;
    return visible;
}

static_assert(next_run_visibility(false, RunState::Active));
static_assert(next_run_visibility(true, RunState::Unknown));
static_assert(!next_run_visibility(true, RunState::Inactive));

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
        visible_ = next_run_visibility(visible_, state);
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
