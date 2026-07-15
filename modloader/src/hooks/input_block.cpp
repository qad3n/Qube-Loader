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
        constexpr char kSetCursorPos[] = "SetCursorPos";

        typedef BOOL(WINAPI* SetCursorPosFn)(int, int);

        std::atomic<bool> g_blocked{false};
        bool g_installed = false;

        SetCursorPosFn g_origSetCursorPos = nullptr;
        void** g_setCursorPosSlot = nullptr;

        // The game recenters the cursor to the client center every frame for infinite mouse-look (its
        // focused-and-no-UI recenter). Swallow it while the overlay owns input so the pointer stays put
        // for the menu; otherwise the game warps it back to center on every frame.
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

        // Patch the user32 SetCursorPos IAT slot the game imports (original via outOrig); false if absent.
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

        if (!hookImport(kSetCursorPos, reinterpret_cast<void*>(&hkSetCursorPos), &g_setCursorPosSlot,
                        reinterpret_cast<void**>(&g_origSetCursorPos)))
        {
            LOGC(Warn, kCategory, "could not hook SetCursorPos; the camera recenter cannot be suppressed while the menu is open");
            return false;
        }

        g_installed = true;
        LOGC(Debug, kCategory, "cursor recenter hook armed (SetCursorPos)");
        return true;
    }

    void remove()
    {
        if (!g_installed)
            return;
        if (g_blocked.exchange(false))
        {
            hooks::dinput::setBlocked(false); // never leave the game's input zeroed on unload
            showCursor(false);
        }

        if (g_setCursorPosSlot && g_origSetCursorPos)
        {
            iat::writeSlot(g_setCursorPosSlot, reinterpret_cast<void*>(g_origSetCursorPos));
            g_setCursorPosSlot = nullptr;
        }

        g_origSetCursorPos = nullptr;
        g_installed = false;

        LOGC(Debug, kCategory, "cursor recenter hook removed");
    }

    void setBlocked(bool blocked)
    {
        if (g_blocked.exchange(blocked) == blocked)
            return; // edge only: nothing to do if already in this state

        // Zero the game's DirectInput keyboard+mouse reads at the source (movement + camera halt), and
        // reveal the OS cursor for the menu (restoring the game's hidden state on close).
        hooks::dinput::setBlocked(blocked);
        showCursor(blocked);
    }

    bool blocked()
    {
        return g_blocked.load();
    }
}
