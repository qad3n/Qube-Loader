#pragma once
// Shared handle to the loader API for every translation unit in this mod.

#include "cube_sdk.h"

namespace exmod
{

    extern const CubeApi* g_api;

    // Shared domain constants used across both the feature and menu layers.
    constexpr int kHexRadix = 16; // addresses/values are parsed and shown as hexadecimal
    constexpr float kFullResource = 1.0f; // a normalized 0..1 resource/stat is full at 1.0

    // Null-guarded wrapper over g_api->log.write, shared by the overlay and the menu tabs.
    void logLine(CubeLogLevel level, const char* message);

}
