#pragma once
// Event sourcing on the render thread: wraps the D3D9/window callbacks into CubeEvents and drives
// the poll-now model (each frame diffs the local player/world and emits edge events).
#include "cube_sdk.h"
#include <windows.h>

struct IDirect3DDevice9;

namespace modloader::gameevents
{
    // Build a bare CubeEventArgs for a lifecycle/edge event and emit it.
    void emitLifecycle(CubeEvent event);

    // Monotonic render-frame counter, reused as a coarse recency stamp for conflict-warning throttles.
    uint32_t currentFrame();

    // Render-dispatch callbacks (match hooks::d3d9::Callbacks signatures).
    void CUBE_CALL onFrame(IDirect3DDevice9* device);
    void CUBE_CALL onDeviceReset(bool preReset);
    bool CUBE_CALL onWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
}