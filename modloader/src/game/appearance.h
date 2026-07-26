#pragma once
// Reads a creature's stored appearance record. The record lives at the start of the ObjWithListMap at
// Creature+0x1d28 (the game deserializes it there and reads it in creature_generateAppearance to build
// the model). Client only, read only; writing appearance persists to the character save.

#include "cube_sdk.h"
#include <cstdint>

namespace game
{
    // Fills out with the appearance of the creature at `address` (0 = local player). Returns true on
    // success; out.valid mirrors the result.
    bool readAppearance(uint32_t address, CubeAppearance& out);
}
