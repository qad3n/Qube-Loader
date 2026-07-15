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

    // "Allow input and movement in menu" (Mod tab): when true, the open menu does NOT freeze the game
    // - movement and camera stay live and the menu is a display-only HUD (the game grabs the cursor for
    // look, so widgets are not clickable in this mode). Reset to false when the menu closes so the next
    // open is the normal interactive menu. setAllowGameInput applies the change immediately.
    void setAllowGameInput(bool allow);
    bool allowGameInput();

}
