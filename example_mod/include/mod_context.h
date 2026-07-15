#pragma once
// Shared handle to the loader API for every translation unit in this mod.

#include "cube_sdk.h"

namespace exmod
{

    extern const CubeApi* g_api;

    // Null-guarded wrapper over g_api->log.write, shared by the overlay and the menu tabs.
    void logLine(CubeLogLevel level, const char* message);

}
