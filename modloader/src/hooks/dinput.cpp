#include "hooks/dinput.h"
#include "hooks/detour.h"
#include "core/log.h"

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <atomic>
#include <cstddef>
#include <cstring>

namespace hooks::dinput
{
    namespace
    {
        constexpr char kCategory[] = "dinput";
        constexpr std::size_t kGetDeviceStateSlot = 9; // IDirectInputDevice8 vtable: Acquire 7, Unacquire 8, GetDeviceState 9
        constexpr DWORD kTeardownDrainMs = 30;

        typedef HRESULT(WINAPI* GetDeviceStateFn)(IDirectInputDevice8W*, DWORD, LPVOID);

        std::atomic<bool> g_blocked{false};
        std::atomic<int> g_inFlight{0};
        bool g_installed = false;
        void* g_getDeviceStateTarget = nullptr;
        GetDeviceStateFn g_origGetDeviceState = nullptr;

        // Runs on the game thread every input poll. Let the real read fill the buffer, then (while the
        // overlay owns input) zero it so the game's keyboard (movement/actions) and mouse (camera look/
        // buttons) reads see nothing. Zero is "no keys down / no mouse delta / no buttons" for both the
        // 256 byte keyboard state and the DIMOUSESTATE, so one memset covers every device.
        HRESULT WINAPI hkGetDeviceState(IDirectInputDevice8W* device, DWORD cbData, LPVOID lpvData)
        {
            g_inFlight.fetch_add(1);
            const HRESULT hr = g_origGetDeviceState(device, cbData, lpvData);

            if (g_blocked.load() && SUCCEEDED(hr) && lpvData && cbData)
                std::memset(lpvData, 0, cbData);

            g_inFlight.fetch_sub(1);
            return hr;
        }

        // Grab the shared IDirectInputDevice8W vtable from a throwaway device (the d3d9 probe pattern);
        // all mouse/keyboard devices share it, so hooking one slot intercepts the game's devices too.
        void** acquireVtable()
        {
            IDirectInput8W* factory = nullptr;
            const HRESULT created = DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION,
                                                       IID_IDirectInput8W, reinterpret_cast<void**>(&factory), nullptr);
            if (FAILED(created) || factory == nullptr)
            {
                LOGC(Error, kCategory, "DirectInput8Create failed (hr 0x%08X)", static_cast<unsigned>(created));
                return nullptr;
            }

            IDirectInputDevice8W* probe = nullptr;
            void** vtable = nullptr;
            const HRESULT device = factory->CreateDevice(GUID_SysMouse, &probe, nullptr);
            if (SUCCEEDED(device) && probe != nullptr)
            {
                vtable = *reinterpret_cast<void***>(probe);
                probe->Release();
            }
            else
                LOGC(Error, kCategory, "probe CreateDevice(SysMouse) failed (hr 0x%08X)", static_cast<unsigned>(device));

            factory->Release();
            return vtable;
        }

    }

    bool install()
    {
        if (g_installed)
            return true;

        void** vtable = acquireVtable();
        if (!vtable)
        {
            LOGC(Warn, kCategory, "could not obtain the DirectInput device vtable; game input will not be blocked");
            return false;
        }

        g_getDeviceStateTarget = vtable[kGetDeviceStateSlot];
        if (!detour::create(g_getDeviceStateTarget, reinterpret_cast<void*>(&hkGetDeviceState),
                            reinterpret_cast<void**>(&g_origGetDeviceState)))
        {
            LOGC(Warn, kCategory, "MinHook install failed for GetDeviceState");
            g_getDeviceStateTarget = nullptr;
            return false;
        }

        g_installed = true;
        LOGC(Debug, kCategory, "GetDeviceState hooked (slot %zu); zeroes the game's input while the menu is open",
             kGetDeviceStateSlot);
        return true;
    }

    void remove()
    {
        if (!g_installed)
            return;

        g_blocked.store(false); // never leave the game's input zeroed if we unload while a menu is open

        // Close the hook path, then drain before MinHook frees the trampoline. mingw has no SEH, so an
        // in flight GetDeviceState call is an inherent (tiny) window.
        detour::remove(g_getDeviceStateTarget);
        while (g_inFlight.load() > 0)
            Sleep(kTeardownDrainMs);

        g_origGetDeviceState = nullptr;
        g_getDeviceStateTarget = nullptr;
        g_installed = false;

        LOGC(Debug, kCategory, "GetDeviceState hook removed");
    }

    void setBlocked(bool blocked)
    {
        g_blocked.store(blocked);
    }
}
