#pragma once
// Reads/writes the local player (CubePlayer) for the player API. Generic Creature helpers and the
// GameController resolution it builds on live in game/creature.h and game/gamecontroller.h.

#include "cube_sdk.h"
#include <cstdint>

namespace game
{
    bool readPlayer(CubePlayer& out);

    // Typed writes into the live local player: resolve + validate the Creature, then write through
    // the guarded mem::write. False if unavailable/unlocated.
    bool addPlayerXp(int32_t amount);
    bool teleportPlayer(float x, float y, float z);
    // Generic scalar setter: maps a CubePlayerStat to its Creature (or GC for facing) field. False if
    // unavailable/unmapped.
    bool setPlayerStat(int32_t stat, double value);
    // Overwrites the local player's inline name buffer (narrow, truncated).
    bool setPlayerName(const char* name);
}
