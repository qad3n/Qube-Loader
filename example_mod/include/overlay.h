#pragma once
// ImGui overlay lifecycle: owns its own ImGui context, draws the menu each frame from the
// device/window the loader hands over via events.

#include "cube_sdk.h"

namespace exmod::overlay
{

    // User UI scale bounds (shared by the overlay clamp and the Mod tab slider).
    constexpr float kMinUiScale = 0.5f;
    constexpr float kMaxUiScale = 3.0f;

    void CUBE_CALL onFrame(CubeEventArgs* args);
    void CUBE_CALL onDeviceReset(CubeEventArgs* args);
    void CUBE_CALL onWndProc(CubeEventArgs* args);
    void CUBE_CALL onShutdown(CubeEventArgs* args);

    // UI scale (Mod tab): effective scale = window DPI * this multiplier; setUiScale reapplies the
    // style next frame. Render thread only.
    void setUiScale(float scale);
    float uiScale();
    float dpiScale();

}
