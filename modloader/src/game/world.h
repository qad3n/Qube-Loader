#pragma once
// Current world/area -> CubeWorld for the world API.

#include "cube_sdk.h"

namespace game
{
    bool readWorld(CubeWorld& out);
    // Fills placed structures / POIs from the player's current zone; returns count.
    int32_t listStructures(CubeStructure* out, int32_t maxCount);
    // Sets the time-of-day in milliseconds (wrapped into a 24h day). False if no world.
    bool setWorldTime(int32_t ms);
    // Writes a CubeTileField on the player's current ZoneTile. False if not resident.
    bool setWorldTile(int32_t fieldId, double value);
    // Sets the world generation seed. False if no world.
    bool setWorldSeed(uint32_t seed);
    // Sets the player's bound spawn point (world units). False if no world.
    bool setWorldSpawn(float x, float y, float z);
    // Writes a CubeStructureField on the structure record at address. False if unreadable.
    bool setStructureField(uint32_t address, int32_t fieldId, double value);
}