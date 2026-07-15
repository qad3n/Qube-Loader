#pragma once
// Session/network state and HUD menu-open state for the mod API.

#include "cube_sdk.h"

namespace game
{
    bool readSession(CubeSession& out);
    bool readUi(CubeUi& out);
    // Writes a CubeSessionField (network mode / connected byte). False if no GC.
    bool setSessionField(int32_t fieldId, int32_t value);
    // Forces a CubeUiField open/closed. False if the chain is unavailable.
    bool setUiField(int32_t fieldId, int32_t open);
}