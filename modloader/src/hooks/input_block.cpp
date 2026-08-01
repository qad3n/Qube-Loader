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
        constexpr char kGetCursorPos[] = "GetCursorPos";

        typedef BOOL(WINAPI* SetCursorPosFn)(int, int);
        typedef BOOL(WINAPI* GetCursorPosFn)(LPPOINT);

        std::atomic<bool> g_blocked{false};
        bool g_installed = false;
        int g_cursorShows = 0; // ShowCursor(TRUE) calls we owe a ShowCursor(FALSE) (game thread only)

        SetCursorPosFn g_origSetCursorPos = nullptr;
        void** g_setCursorPosSlot = nullptr;
        GetCursorPosFn g_origGetCursorPos = nullptr;
        void** g_getCursorPosSlot = nullptr;

        // The last spot the game asked the cursor to sit at, i.e. its mouse look anchor (the client
        // center). Recorded on every SetCursorPos, blocked or not, so the value is always the one the
        // game itself computed. Screen coordinates.
        std::atomic<bool> g_anchorValid{false};
        std::atomic<int> g_anchorX{0};
        std::atomic<int> g_anchorY{0};

        // The game recenters the cursor to the client center every frame for infinite mouse look.
        // Swallow that while the overlay owns input so the pointer stays put for the menu; otherwise the
        // game warps it back to center on every frame. Keep recording the anchor either way: it is what
        // hkGetCursorPos reports while blocked, and where the pointer is restored to on unblock.
        BOOL WINAPI hkSetCursorPos(int x, int y)
        {
            g_anchorX.store(x, std::memory_order_relaxed);
            g_anchorY.store(y, std::memory_order_relaxed);
            g_anchorValid.store(true, std::memory_order_release);

            if (g_blocked.load())
                return TRUE;
            if (g_origSetCursorPos)
                return g_origSetCursorPos(x, y);

            return FALSE;
        }

        // The other half of the game's mouse look: it reads the pointer and treats the offset from its
        // anchor as the camera delta. While blocked, hand it the anchor so that offset is exactly zero
        // every frame - the camera holds perfectly still no matter where the user drags the pointer over
        // the menu. Only the game's import is patched, so the loader's own GetCursorPos calls (ImGui's
        // mouse position among them) still see the real pointer.
        BOOL WINAPI hkGetCursorPos(LPPOINT point)
        {
            if (g_blocked.load() && point != nullptr && g_anchorValid.load(std::memory_order_acquire))
            {
                point->x = g_anchorX.load(std::memory_order_relaxed);
                point->y = g_anchorY.load(std::memory_order_relaxed);
                return TRUE;
            }
            if (g_origGetCursorPos)
                return g_origGetCursorPos(point);

            return FALSE;
        }

        // ShowCursor keeps a per process counter, not a flag: the cursor is drawn while it is >= 0 and
        // the game holds it negative. Raise it to 0 to reveal the pointer for the menu, remembering how
        // many increments that took so closing the menu puts the game's own count back EXACTLY (a fixed
        // single decrement would strand the counter at -1 whenever the game had hidden it more deeply).
        void showCursor(bool visible)
        {
            if (visible)
            {
                // Count EVERY call, including the one that reaches >= 0: it is the call that actually
                // revealed the pointer, and leaving it unpaired is what would strand the counter at 0
                // and keep the cursor drawn over the game after the menu closed.
                int count = -1;
                do
                {
                    count = ShowCursor(TRUE);
                    ++g_cursorShows;
                } while (count < 0);
            }
            else
            {
                while (g_cursorShows > 0)
                {
                    ShowCursor(FALSE);
                    --g_cursorShows;
                }
            }
        }

        // Patch one user32 IAT slot the game imports (original via outOrig); false if absent.
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

        void unhookImport(void*** slot, void** orig)
        {
            if (*slot && *orig)
                iat::writeSlot(*slot, *orig);
            *slot = nullptr;
            *orig = nullptr;
        }

    }

    bool install()
    {
        if (g_installed)
            return true;

        if (!hookImport(kSetCursorPos, reinterpret_cast<void*>(&hkSetCursorPos), &g_setCursorPosSlot,
                        reinterpret_cast<void**>(&g_origSetCursorPos)))
        {
            LOGC(Warn, kCategory,
                 "could not hook SetCursorPos; the camera recenter cannot be suppressed while the menu is "
                 "open");
            return false;
        }

        // Optional half: without it the pointer is still freed for the menu, but the game keeps reading
        // the real one, so the camera drifts while a menu is open and jumps when it closes. Warn loudly
        // rather than fail, since the freeze is still a large improvement over nothing.
        if (!hookImport(kGetCursorPos, reinterpret_cast<void*>(&hkGetCursorPos), &g_getCursorPosSlot,
                        reinterpret_cast<void**>(&g_origGetCursorPos)))
            LOGC(Warn, kCategory,
                 "could not hook GetCursorPos; the camera may drift while the menu is open and jump when it "
                 "closes");

        g_installed = true;
        LOGC(Debug, kCategory, "mouse look hooks armed (SetCursorPos%s)",
             g_origGetCursorPos ? " + GetCursorPos" : " only");
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

        unhookImport(&g_setCursorPosSlot, reinterpret_cast<void**>(&g_origSetCursorPos));
        unhookImport(&g_getCursorPosSlot, reinterpret_cast<void**>(&g_origGetCursorPos));
        g_anchorValid.store(false, std::memory_order_release);
        g_installed = false;

        LOGC(Debug, kCategory, "mouse look hooks removed");
    }

    void setBlocked(bool blocked)
    {
        if (g_blocked.exchange(blocked) == blocked)
            return; // edge only: nothing to do if already in this state

        if (blocked)
        {
            // Zero the game's DirectInput keyboard+mouse reads at the source: that is what actually
            // stops movement AND the camera, which the game drives off the mouse's relative axes.
            // Then reveal the OS cursor for the menu.
            hooks::dinput::setBlocked(true);
            showCursor(true);
            return;
        }

        // Closing: undo in reverse. Do NOT warp the pointer here. The game recenters it itself on its
        // next look mode frame, and every warp is one more relative delta for DirectInput to report;
        // dinput's post release settle is what keeps that catch up jump out of the camera.
        showCursor(false);
        hooks::dinput::setBlocked(false);
    }
}
