#include "hooks/input_block.h"
#include "hooks/dinput.h"
#include "core/iat.h"
#include "core/log.h"

#include <windows.h>

#include <atomic>

namespace hooks::input_block
{
    namespace
    {
        constexpr char kCategory[] = "input";
        constexpr char kUser32[] = "user32.dll";
        constexpr char kGetFocus[] = "GetFocus";
        constexpr char kSetCursorPos[] = "SetCursorPos";

        typedef HWND(WINAPI* GetFocusFn)();
        typedef BOOL(WINAPI* SetCursorPosFn)(int, int);

        std::atomic<bool> g_blocked{false};
        bool g_installed = false;

        GetFocusFn g_origGetFocus = nullptr;
        void** g_getFocusSlot = nullptr;

        SetCursorPosFn g_origSetCursorPos = nullptr;
        void** g_setCursorPosSlot = nullptr;

        // Returning a non-window handle while blocked makes the game skip its GetFocus-gated
        // input+recenter block, halting WASD, mouse-look and cursor warp at once.
        HWND WINAPI hkGetFocus()
        {
            if (g_blocked.load())
                return nullptr;
            if (g_origGetFocus)
                return g_origGetFocus();

            return nullptr;
        }

        // Defense in depth: the game's cursor-mode entry recenter is not behind GetFocus, so
        // swallow SetCursorPos while blocked to guarantee the cursor never warps under the menu.
        BOOL WINAPI hkSetCursorPos(int x, int y)
        {
            if (g_blocked.load())
                return TRUE;
            if (g_origSetCursorPos)
                return g_origSetCursorPos(x, y);

            return FALSE;
        }

        void showCursor(bool visible)
        {
            if (visible)
                while (ShowCursor(TRUE) < 0);
            else
                ShowCursor(FALSE); // restore the game's hidden-cursor state (it did ShowCursor(0))
        }

        // Patch one user32 IAT slot the game imports (original via outOrig); false if absent (logged).
        bool hookImport(const char* funcName, void* replacement, void*** outSlot, void** outOrig)
        {
            HMODULE user32 = GetModuleHandleA(kUser32);
            void* real = iat::resolveImport(user32, kUser32, funcName);
            if (!real)
                return false;

            void* orig = iat::patchIatSlot(kUser32, funcName, real, replacement, outSlot);
            if (!orig)
                return false;

            *outOrig = orig;
            return true;
        }

    }

    bool install()
    {
        if (g_installed)
            return true;

        if (!hookImport(kGetFocus, reinterpret_cast<void*>(&hkGetFocus), &g_getFocusSlot, reinterpret_cast<void**>(&g_origGetFocus)))
        {
            LOGC(Warn, kCategory, "could not hook GetFocus; input freeze disabled");
            return false;
        }

        // Best-effort: the GetFocus gate already covers the main recenter path.
        if (!hookImport(kSetCursorPos, reinterpret_cast<void*>(&hkSetCursorPos), &g_setCursorPosSlot,
                        reinterpret_cast<void**>(&g_origSetCursorPos)))
            LOGC(Debug, kCategory, "SetCursorPos not hooked (cursor covered by GetFocus gate)");

        g_installed = true;
        LOGC(Debug, kCategory, "input freeze armed (GetFocus gate%s)",
             g_setCursorPosSlot ? " + cursor recenter" : "");
        return true;
    }

    void remove()
    {
        if (!g_installed)
            return;
        if (g_blocked.exchange(false))
            showCursor(false); // never leave the OS cursor forced-visible if we unload while blocked

        if (g_getFocusSlot && g_origGetFocus)
        {
            iat::writeSlot(g_getFocusSlot, reinterpret_cast<void*>(g_origGetFocus));
            g_getFocusSlot = nullptr;
        }
        if (g_setCursorPosSlot && g_origSetCursorPos)
        {
            iat::writeSlot(g_setCursorPosSlot, reinterpret_cast<void*>(g_origSetCursorPos));
            g_setCursorPosSlot = nullptr;
        }

        g_origGetFocus = nullptr;
        g_origSetCursorPos = nullptr;
        g_installed = false;

        LOGC(Debug, kCategory, "input freeze removed");
    }

    void setBlocked(bool blocked)
    {
        // Act only on the edge. The DI suspend is what actually restores overlay clicks under Wine
        // (unacquiring the game's mouse lets window messages flow); the GetFocus gate freezes the
        // game meanwhile. No cursor clip/recenter: it only fought the Wayland compositor.
        if (g_blocked.exchange(blocked) == blocked)
            return;

        hooks::dinput::setAcquired(!blocked);
        showCursor(blocked);
    }

    bool blocked()
    {
        return g_blocked.load();
    }
}
