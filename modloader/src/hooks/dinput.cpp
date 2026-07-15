#include "hooks/dinput.h"
#include "hooks/detour.h"
#include "core/log.h"

#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>

#include <atomic>
#include <cstddef>

namespace hooks::dinput
{
    namespace
    {
        constexpr char kCategory[] = "dinput";
        constexpr std::size_t kGetDeviceStateSlot = 9; // IDirectInputDevice8 vtable: Acquire 7, Unacquire 8, GetDeviceState 9
        constexpr int kMaxDevices = 4; // mouse + keyboard (+ headroom); the game creates two
        constexpr DWORD kTeardownDrainMs = 30;

        typedef HRESULT(WINAPI* GetDeviceStateFn)(IDirectInputDevice8W*, DWORD, LPVOID);

        // Borrowed pointers: the game owns these devices for the whole session (it never releases or
        // re-creates them), so we cache the raw pointers without an AddRef.
        std::atomic<IDirectInputDevice8W*> g_devices[kMaxDevices] = {};
        std::atomic<bool> g_suspended{false};
        bool g_installed = false;
        void* g_getDeviceStateTarget = nullptr;
        GetDeviceStateFn g_origGetDeviceState = nullptr;

        // Runs on the game thread every polled frame; record each distinct device once.
        void captureDevice(IDirectInputDevice8W* device)
        {
            if (!device)
                return;
            for (int i = 0; i < kMaxDevices; ++i)
            {
                IDirectInputDevice8W* current = g_devices[i].load();
                if (current == device)
                    return;
                if (current == nullptr)
                {
                    g_devices[i].store(device);
                    return;
                }
            }
        }

        HRESULT WINAPI hkGetDeviceState(IDirectInputDevice8W* device, DWORD cbData, LPVOID lpvData)
        {
            captureDevice(device);
            return g_origGetDeviceState(device, cbData, lpvData);
        }

        // Grab the shared IDirectInputDevice8W vtable from a throwaway device (the d3d9-probe pattern);
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
            LOGC(Warn, kCategory, "could not obtain the DirectInput device vtable; overlay clicks may be lost");
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
        LOGC(Debug, kCategory, "GetDeviceState hooked (slot %zu); will capture the game's DI devices",
             kGetDeviceStateSlot);
        return true;
    }

    void remove()
    {
        if (!g_installed)
            return;

        setAcquired(true); // never leave the game's devices unacquired if we unload while a menu is open

        // Close the hook path, then drain before MinHook frees the trampoline. wine: mingw has no SEH,
        // so an in-flight GetDeviceState call is an inherent (tiny) window.
        detour::remove(g_getDeviceStateTarget);
        Sleep(kTeardownDrainMs);

        for (int i = 0; i < kMaxDevices; ++i)
            g_devices[i].store(nullptr);
        g_origGetDeviceState = nullptr;
        g_getDeviceStateTarget = nullptr;
        g_installed = false;

        LOGC(Debug, kCategory, "GetDeviceState hook removed");
    }

    void setAcquired(bool acquired)
    {
        const bool suspend = !acquired;
        if (g_suspended.exchange(suspend) == suspend)
            return; // edge only: nothing to do if already in this state

        int touched = 0;
        for (int i = 0; i < kMaxDevices; ++i)
        {
            IDirectInputDevice8W* device = g_devices[i].load();
            if (!device)
                continue;
            if (suspend)
                device->Unacquire();
            else
                device->Acquire();
            ++touched;
        }
        LOGC(Debug, kCategory, "%s %d DI device(s)", suspend ? "unacquired" : "re-acquired", touched);
    }
}
