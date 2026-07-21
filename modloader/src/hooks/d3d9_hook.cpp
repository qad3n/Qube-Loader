#include "hooks/d3d9_hook.h"
#include "hooks/detour.h"
#include "hooks/window.h"
#include "core/log.h"
#include "core/mem.h"
#include "util/fmt.h"

#include <atomic>
#include <cstddef>

namespace hooks::d3d9
{
    namespace
    {
        constexpr char kCategory[] = "d3d9";
        constexpr char kProbeClass[] = "CubeModD3DProbe";
        constexpr DWORD kTeardownDrainMs = 100;
        constexpr int kProbeWindowSize = 64;

        // IDirect3DDevice9 vtable slots we intercept (stable across d3d9 and Wine).
        enum class Slot : std::size_t
        {
            Reset = 16,
            EndScene = 42
        };

        typedef HRESULT(WINAPI* EndSceneFn)(IDirect3DDevice9*);
        typedef HRESULT(WINAPI* ResetFn)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

        Callbacks g_cb;
        void** g_vtable = nullptr;
        void* g_endSceneTarget = nullptr; // MinHook hooks by address
        void* g_resetTarget = nullptr;
        EndSceneFn g_origEndScene = nullptr; // MinHook trampoline
        ResetFn g_origReset = nullptr;
        std::atomic<bool> g_active{false};
        bool g_windowHooked = false; // render thread only

        // Borderless coercion state (render thread only). The game runs in D3D9 EXCLUSIVE fullscreen,
        // which makes its window topmost + non-minimizable and loses the device on every alt-tab. We
        // force it to borderless windowed so alt-tab / minimize work and no exclusive device loss can
        // freeze it. Cache the original style ONCE so eject can restore the game's own window.
        bool g_styleSaved = false;
        LONG_PTR g_savedStyle = 0;
        LONG_PTR g_savedExStyle = 0;
        HWND g_styledWindow = nullptr;

        std::size_t slotIndex(Slot slot)
        {
            return static_cast<std::size_t>(slot);
        }

        // Convert the game window to borderless covering its monitor: not topmost (alt-tab works),
        // WS_POPUP (minimizable, no title bar), sized to the monitor. Idempotent; caches the original
        // style the first time so restoreWindowStyle can put it back on eject.
        void applyBorderless(HWND hwnd)
        {
            if (hwnd == nullptr)
                return;

            const LONG_PTR style = GetWindowLongPtrA(hwnd, GWL_STYLE);
            const LONG_PTR exStyle = GetWindowLongPtrA(hwnd, GWL_EXSTYLE);
            if (!g_styleSaved)
            {
                g_savedStyle = style;
                g_savedExStyle = exStyle;
                g_styledWindow = hwnd;
                g_styleSaved = true;
            }

            const LONG_PTR newStyle = (style & ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW)) | WS_POPUP | WS_VISIBLE;
            const LONG_PTR newExStyle = exStyle & ~static_cast<LONG_PTR>(WS_EX_TOPMOST);
            SetWindowLongPtrA(hwnd, GWL_STYLE, newStyle);
            SetWindowLongPtrA(hwnd, GWL_EXSTYLE, newExStyle);

            RECT mon = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
            HMONITOR hm = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = {};
            mi.cbSize = sizeof(mi);
            if (hm != nullptr && GetMonitorInfoA(hm, &mi))
                mon = mi.rcMonitor;

            // HWND_NOTOPMOST drops the exclusive-fullscreen topmost band; SWP_NOACTIVATE so we do not
            // steal focus, SWP_FRAMECHANGED so the style change takes effect.
            SetWindowPos(hwnd, HWND_NOTOPMOST, mon.left, mon.top, mon.right - mon.left, mon.bottom - mon.top,
                         SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            LOGC(Info, kCategory, "forced borderless windowed on hwnd 0x%X (%ldx%ld) to fix exclusive-fullscreen alt-tab/minimize/freeze",
                 fmt::ptr(hwnd), static_cast<long>(mon.right - mon.left), static_cast<long>(mon.bottom - mon.top));
        }

        void restoreWindowStyle()
        {
            if (!g_styleSaved || g_styledWindow == nullptr)
                return;
            SetWindowLongPtrA(g_styledWindow, GWL_STYLE, g_savedStyle);
            SetWindowLongPtrA(g_styledWindow, GWL_EXSTYLE, g_savedExStyle);
            SetWindowPos(g_styledWindow, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            g_styleSaved = false;
            g_styledWindow = nullptr;
        }

        // Learn the window handle from the device (first frame) and hand it to hooks::window.
        void ensureWindowHook(IDirect3DDevice9* device)
        {
            if (g_windowHooked)
                return;
            g_windowHooked = true; // one attempt only, whatever the outcome

            D3DDEVICE_CREATION_PARAMETERS params = {};
            if (FAILED(device->GetCreationParameters(&params)) || params.hFocusWindow == nullptr)
            {
                LOGC(Warn, kCategory, "GetCreationParameters failed; input hook unavailable");
                return;
            }

            hooks::window::ensureHook(params.hFocusWindow, g_cb.onWndProc);
        }

        HRESULT WINAPI hkEndScene(IDirect3DDevice9* device)
        {
            if (g_active.load())
            {
                ensureWindowHook(device);
                // Gate ONLY the draw on the cooperative level. In exclusive fullscreen the device goes
                // D3DERR_DEVICELOST / D3DERR_DEVICENOTRESET on focus loss / mode change; drawing ImGui
                // (default-pool resources) on a lost device is what freezes/crashes the game. When lost
                // we simply drop the overlay frame - the game's own loop performs the Reset, which
                // hkReset catches (invalidate -> reset -> recreate). Skipping the draw (not the hook)
                // keeps a wrapper false-positive cheap: one dropped frame, never a permanently hidden
                // overlay. The trampoline below still runs every call so the game frame is untouched.
                if (g_cb.onRender != nullptr && device->TestCooperativeLevel() == D3D_OK)
                    g_cb.onRender(device);
            }
            return g_origEndScene(device);
        }

        HRESULT WINAPI hkReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pp)
        {
            // Force borderless windowed: if the game is resetting into exclusive fullscreen, flip the
            // present params to windowed so the device never takes exclusive mode again. The game already
            // sized the back buffer to the target resolution, so we leave BackBufferWidth/Height alone and
            // only clear the fullscreen bits. The matching borderless window restyle happens after the
            // reset succeeds (below). This catches every reset: startup settings-apply, alt-tab device-loss
            // recovery, and in-game resolution/mode changes.
            bool coerced = false;
            if (g_active.load() && pp != nullptr && pp->Windowed == FALSE)
            {
                pp->Windowed = TRUE;
                pp->FullScreen_RefreshRateInHz = 0;
                coerced = true;
            }

            const bool live = g_active.load() && g_cb.onDeviceReset != nullptr;
            if (live)
                g_cb.onDeviceReset(true);

            const HRESULT hr = g_origReset(device, pp);
            if (SUCCEEDED(hr) && coerced)
                applyBorderless(hooks::window::window());
            if (live && SUCCEEDED(hr))
                g_cb.onDeviceReset(false);

            return hr;
        }

        bool acquireVtable()
        {
            HINSTANCE self = GetModuleHandleA(nullptr);

            WNDCLASSEXA wc = {};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = DefWindowProcA;
            wc.hInstance = self;
            wc.lpszClassName = kProbeClass;

            if (RegisterClassExA(&wc) == 0)
            {
                LOGC(Error, kCategory, "probe window class registration failed");
                return false;
            }

            HWND probeWnd = CreateWindowExA(0, kProbeClass, kProbeClass, WS_OVERLAPPEDWINDOW,
                                            0, 0, kProbeWindowSize, kProbeWindowSize,
                                            nullptr, nullptr, self, nullptr);
            if (probeWnd == nullptr)
            {
                LOGC(Error, kCategory, "probe window creation failed");
                UnregisterClassA(kProbeClass, self);
                return false;
            }

            IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
            if (d3d == nullptr)
            {
                LOGC(Error, kCategory, "Direct3DCreate9 returned null");
                DestroyWindow(probeWnd);
                UnregisterClassA(kProbeClass, self);
                return false;
            }

            // Log the adapter/driver before creating the device: this identifies the GPU and, crucially,
            // any d3d9 wrapper (DXVK / dgVoodoo / overlay) that changes the device layout, which is the
            // environment detail that explains an overlay problem on a given machine.
            D3DADAPTER_IDENTIFIER9 adapter = {};
            if (SUCCEEDED(d3d->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adapter)))
                LOGC(Info, kCategory, "D3D9 adapter: %s | driver %s %u.%u.%u.%u | vendor 0x%04X device 0x%04X",
                     adapter.Description, adapter.Driver,
                     HIWORD(adapter.DriverVersion.HighPart), LOWORD(adapter.DriverVersion.HighPart),
                     HIWORD(adapter.DriverVersion.LowPart), LOWORD(adapter.DriverVersion.LowPart),
                     static_cast<unsigned>(adapter.VendorId), static_cast<unsigned>(adapter.DeviceId));

            D3DPRESENT_PARAMETERS pp = {};
            pp.Windowed = TRUE;
            pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
            pp.hDeviceWindow = probeWnd;

            IDirect3DDevice9* probeDev = nullptr;
            const HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, probeWnd,
                                                 D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &probeDev);
            if (SUCCEEDED(hr) && probeDev != nullptr)
            {
                void** vtable = *reinterpret_cast<void***>(probeDev);
                g_vtable = vtable;
                // Capture the EndScene/Reset addresses WHILE the probe device is alive. Some d3d9
                // wrappers (DXVK / dgVoodoo / injected overlays) heap-allocate a per-device vtable and
                // free it on Release, so reading a slot after Release dereferences freed memory (observed
                // AV reading slot 42). Guard the read too, in case the vtable is shorter than the
                // standard layout - a nonstandard d3d9 then disables the overlay instead of crashing.
                const size_t needed = sizeof(void*) * (slotIndex(Slot::EndScene) + 1);
                if (vtable != nullptr && mem::readable(vtable, needed))
                {
                    g_endSceneTarget = vtable[slotIndex(Slot::EndScene)];
                    g_resetTarget = vtable[slotIndex(Slot::Reset)];
                }
                else
                    LOGC(Error, kCategory, "device vtable unreadable (nonstandard d3d9 wrapper?); overlay disabled");
                probeDev->Release();
            }
            else
                LOGC(Error, kCategory, "probe CreateDevice failed (hr 0x%08X)", static_cast<unsigned>(hr));

            d3d->Release();
            DestroyWindow(probeWnd);
            UnregisterClassA(kProbeClass, self);

            return g_endSceneTarget != nullptr && g_resetTarget != nullptr;
        }

    }

    bool install(const Callbacks& callbacks)
    {
        if (g_active.load())
            return true;

        g_cb = callbacks;
        if (!acquireVtable())
        {
            LOGC(Error, kCategory, "could not obtain the D3D9 device vtable");
            return false;
        }
        LOGC(Debug, kCategory, "device vtable acquired at 0x%X (EndScene 0x%X, Reset 0x%X)",
             fmt::ptr(g_vtable), fmt::ptr(g_endSceneTarget), fmt::ptr(g_resetTarget));

        // g_endSceneTarget / g_resetTarget were captured in acquireVtable while the probe device was
        // alive. The probe shares these function addresses with the game's device, so inline-hooking
        // them intercepts the game too.
        const bool endSceneOk = detour::create(g_endSceneTarget, reinterpret_cast<void*>(&hkEndScene),
                                               reinterpret_cast<void**>(&g_origEndScene));
        const bool resetOk = detour::create(g_resetTarget, reinterpret_cast<void*>(&hkReset),
                                            reinterpret_cast<void**>(&g_origReset));
        if (!endSceneOk || !resetOk)
        {
            if (endSceneOk)
                detour::remove(g_endSceneTarget);
            if (resetOk)
                detour::remove(g_resetTarget);
            // No detour::shutdown() here: MinHook is shared with input_block.
            g_endSceneTarget = nullptr;
            g_resetTarget = nullptr;
            g_origEndScene = nullptr;
            g_origReset = nullptr;
            LOGC(Error, kCategory, "MinHook install failed for EndScene/Reset");
            return false;
        }

        g_active.store(true);
        LOGC(Debug, kCategory, "hooked EndScene (slot %u) and Reset (slot %u) via MinHook", static_cast<unsigned>(slotIndex(Slot::EndScene)), static_cast<unsigned>(slotIndex(Slot::Reset)));
        return true;
    }

    void remove()
    {
        if (!g_active.load())
            return;

        // Close the hook path, then drain before MinHook frees the trampolines (hooks still tail-call
        // the trampoline when inactive). wine: mingw has no SEH, so the in-flight window is inherent.
        g_active.store(false);
        restoreWindowStyle(); // put the game's own window style back before we drop the WndProc hook
        hooks::window::restore();
        Sleep(kTeardownDrainMs);
        detour::remove(g_endSceneTarget);
        detour::remove(g_resetTarget);
        // MinHook shutdown lives in loader teardown (shared with input_block), not here.

        g_origEndScene = nullptr;
        g_origReset = nullptr;
        g_endSceneTarget = nullptr;
        g_resetTarget = nullptr;
        g_vtable = nullptr;
        g_windowHooked = false;

        LOGC(Debug, kCategory, "D3D9 hooks removed");
    }

    HWND window()
    {
        return hooks::window::window();
    }
}