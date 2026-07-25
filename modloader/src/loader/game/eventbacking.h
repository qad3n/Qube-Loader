#pragma once
// What each mod facing feature needs PATCHED into the game, and a refcount over it.
//
// The loader detours a game function only because a mod asked: a subscription to a detour backed
// event, or a call to a pull API that has nothing to return until its capture is armed. Nothing is
// installed at setup, so with no mod loaded (or a mod that only reads state) the game binary is
// untouched. Poll backed events read memory and appear here not at all.
//
// One refcount per BACKING, not per event, because several features share one detour (the R-select
// capture backs both CUBE_EVENT_ENTITY_SELECTED and selection.getLast). The zero to one edge installs,
// the one to zero edge removes.

#include "cube_sdk.h"

namespace modloader::eventbacking
{
    // A patchable resource a feature can depend on.
    enum class Backing
    {
        Selection, // game::selection capture (R / use key)
        Pickup, // game::pickup capture (E / hold to pickup)
        CritRoll, // CUBE_HOOK_CRIT_ROLL observation (pass through crit counting)
        Count
    };

    // Refcounted arm/disarm. acquire() installs on the zero to one edge and returns whether the
    // backing is installed afterwards; release() removes on the one to zero edge.
    bool acquire(Backing backing);
    void release(Backing backing);

    // Retain/release everything `event` needs. Called from the event bus on the per event subscriber
    // count edges; an event with no backing is a no op.
    void retainEvent(CubeEvent event);
    void releaseEvent(CubeEvent event);

    // Teardown: drop every outstanding reference and remove whatever is still installed.
    void releaseAll();
}
