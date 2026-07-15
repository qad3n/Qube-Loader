#pragma once
// Subclasses the game window's WndProc so input messages reach the loader. The D3D9 hook discovers
// the window handle from the device and hands it here.
#include "hooks/d3d9_hook.h" // hooks::d3d9::WndProcFn (the shared swallow callback)

namespace hooks::window
{
    // Subclass hwnd once, routing each message to onWndProc (true swallows it). Idempotent.
    bool ensureHook(HWND hwnd, d3d9::WndProcFn onWndProc);
    // Restore the original WndProc and stop dispatching. Safe when not hooked.
    void restore();
    // The subclassed window, or null when not hooked.
    HWND window();
}