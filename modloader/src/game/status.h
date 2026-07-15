#pragma once
// Status effects / buffs: intrusive circular list at Creature+0x1178.

#include "cube_sdk.h"
#include <cstdint>

namespace game
{
    int32_t listBuffs(CubeBuff* out, int32_t maxCount);
    // Any creature's status list (generic Creature field: works for entity/pet too).
    int32_t listBuffsAt(uintptr_t creature, CubeBuff* out, int32_t maxCount);
    // Status effects of the creature at address (validated first).
    int32_t listBuffsOfAddress(uint32_t address, CubeBuff* out, int32_t maxCount);
    // Writes a CubeBuffField on the effect node at address.
    bool setBuffField(uint32_t address, int32_t fieldId, double value);
    // Stun / knocked-down snapshot for the creature at address (0 = local player).
    bool readStun(uint32_t address, CubeStun& out);
    // Clears the stun on the creature at address (0 = local player).
    bool clearStun(uint32_t address);
}