#pragma once
// Resolves the local player's active pet (a live Creature) into a CubeCompanion.

#include "cube_sdk.h"

namespace game
{
    bool readCompanion(CubeCompanion& out);
}