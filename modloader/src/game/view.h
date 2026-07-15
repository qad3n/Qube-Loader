#pragma once
// Client-only reads: 3rd-person camera scalars and the render target size.

#include "cube_sdk.h"

namespace game
{
    bool readCamera(CubeCamera& out);
    bool readDisplay(CubeDisplay& out);
    // Live audio state (config volumes + music-streamer playing/looping/volume, guarded).
    bool readAudio(CubeAudio& out);
    // Writes a CubeCameraField (distance/pitch/yaw) on the GameController camera.
    bool setCameraField(int32_t fieldId, double value);
    // Writes a CubeDisplayField (the options.cfg global settings). False if unmapped.
    bool setDisplayField(int32_t fieldId, int32_t value);
}
