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

        std::size_t slotIndex(Slot slot)
        {
            return static_cast<std::size_t>(slot);
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
                // The game only calls EndScene mid-frame on a valid device, so drawing here is safe in
                // windowed AND exclusive fullscreen (this is what every injected overlay does). Device
                // loss is handled where it belongs, in hkReset; a bad draw is caught by the render
                // dispatch's fault isolation, never a crash. No TestCooperativeLevel gate: it reports
                // spuriously under some fullscreen/wrapper setups and would wrongly hide the overlay.
                if (g_cb.onRender != nullptr)
                    g_cb.onRender(device);
            }
            return g_origEndScene(device);
        }

        HRESULT WINAPI hkReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pp)
        {
            const bool live = g_active.load() && g_cb.onDeviceReset != nullptr;
            if (live)
                g_cb.onDeviceReset(true);

            const HRESULT hr = g_origReset(device, pp);
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