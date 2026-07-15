#pragma once
// Resolves the local player's active pet (a live Creature) into a CubePet.

#include "cube_sdk.h"

namespace game
{
    bool readPet(CubePet& out);
}