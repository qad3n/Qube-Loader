#pragma once
// Nearby-entity enumeration + creature-by-id lookup.

#include "cube_sdk.h"
#include <cstdint>

namespace game
{

    int32_t listEntities(CubeEntity* out, int32_t maxCount);
    bool targetEntity(CubeEntity& out);
    // Resolves the crosshair aim/hover target to a live entity.
    bool aimTargetEntity(CubeEntity& out);
    bool findCreatureById(uintptr_t gc, uint64_t id, uintptr_t& creatureOut);
    // Counts player-category creatures in the entity map (includes the local player).
    int32_t countPlayers(uintptr_t gc);
    // True if the creature at address is a tameable passive critter (game predicate FUN_00444680).
    bool isCreatureTameable(uint32_t address);
    // Edits the creature at address (validated first). Works on any entity/pet/target.
    bool setEntityStat(uint32_t address, int32_t stat, double value);
    bool setEntityName(uint32_t address, const char* name);
    bool teleportEntity(uint32_t address, float x, float y, float z);
}