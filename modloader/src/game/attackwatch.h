#pragma once
// Game-thread attack/ability detection. The local player's attack action byte can be set and cleared
// within a single game tick (a bow shot, an instant ability), which the once-per-frame render poll
// almost always misses. This samples the action on the game thread (from the per-tick behavior detour)
// so the edge is caught the instant it happens; gameevents emits PLAYER_ATTACK from pollAttack.

#include <cstdint>

namespace game::attackwatch
{
    // Render thread: which Creature is the local player (0 = none), the cheap game-thread filter.
    void setLocalPlayer(uint32_t creature);
    // Enable/report whether the behavior-tick detour is installed (else gameevents falls back to polling).
    void setActive(bool on);
    bool active();
    // Game thread (behavior-tick detour): sample the ticked creature; if it is the local player and it
    // just entered a combat action, record an attack edge.
    void onBehaviorTick(uint32_t creature);
    // Render thread: if attack edges were recorded since the last poll, returns true with the action id
    // captured at the most recent edge and how many edges occurred; advances the seen counter.
    bool pollAttack(int32_t& actionId, uint32_t& count);
}
