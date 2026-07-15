#pragma once
// Hooks the D3D9 device (EndScene/Reset) via its shared vtable plus the window WndProc for input.

#include <windows.h>
#include <d3d9.h>

namespace hooks::d3d9
{
    typedef void (*RenderFn)(IDirect3DDevice9* device);
    typedef void (*DeviceResetFn)(bool preReset); // true: about to reset; false: reset done
    typedef bool (*WndProcFn)(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam); // true: swallow message

    struct Callbacks
    {
        RenderFn onRender = nullptr;
        DeviceResetFn onDeviceReset = nullptr;
        WndProcFn onWndProc = nullptr;
    };

    bool install(const Callbacks& callbacks);
    void remove();
    HWND window();
}