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
    bool hold(GameStats& out) const
    {
        if (visible_) out = last_;
        return visible_;
    }

    bool update(GameStats& out, RunState state)
    {
        visible_ = next_run_visibility(visible_, state);
        if (visible_) last_ = out;
        return visible_;
    }

private:
    bool visible_ = false;
    GameStats last_{};
};

} // namespace bb
