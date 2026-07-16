#pragma once
// Local-player control-lock classifier. The Creature +0x128 timer (0..600) is a SHARED "cannot act"
// lock the game sets for BOTH a real hit-stun AND a self-initiated dodge-roll (FUN_004a6b50 spends
// stamina, applies a dash + upward pop, then sets +0x128=600). A single snapshot cannot tell them
// apart, so this samples the timer edge each frame and classifies a fresh lock by whether the player
// lost health (hit-stun) or not (roll from the ground). One owner of the roll/stun verdict, consulted
// by the player read (getAction -> Rolling) and the event poller (suppress the roll's false jump/stun).
#include <cstdint>

namespace game::actionlock
{
    enum class Cause : int32_t
    {
        None = 0,
        Rolling,
        Stunned
    };

    // Sample the local player's lock timer and reclassify on its rising edge. Reads health / lock
    // timer / ground-contact from the creature itself. Idempotent within a frame (memory is static
    // per frame, so repeat calls re-detect the same edge and latch the same verdict). Call before
    // reading the classification. A changed player base (respawn / relog) resets the tracked deltas.
    void sample(uintptr_t localPlayer);

    Cause cause();
    bool rolling();
    bool stunned();

    // The creature base the current verdict belongs to (0 until first sampled); lets a reader confirm
    // an address is the classified local player before trusting rolling()/stunned().
    uintptr_t subject();
}
