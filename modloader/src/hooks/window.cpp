#include "hooks/window.h"
#include "core/log.h"
#include "util/fmt.h"

#include <atomic>

namespace hooks::window
{
    namespace
    {
        constexpr char kCategory[] = "window";

        WNDPROC g_origWndProc = nullptr;
        HWND g_window = nullptr;
        d3d9::WndProcFn g_onWndProc = nullptr;
        std::atomic<bool> g_active{false};

        // What a swallowed message must report back to the game. Win32 defines a meaningful non zero
        // result for only a few of the messages the overlay eats: TRUE means "handled, stop here" for
        // WM_SETCURSOR and the XBUTTON pair. Every other input message documents 0 as "processed".
        LRESULT swallowResult(UINT msg)
        {
            switch (msg)
            {
                case WM_SETCURSOR:
                case WM_XBUTTONDOWN:
                case WM_XBUTTONUP:
                case WM_XBUTTONDBLCLK:
                    return TRUE;
                default:
                    return 0;
            }
        }

        LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            const d3d9::WndProcFn cb = g_onWndProc;
            if (g_active.load() && cb != nullptr && cb(hwnd, msg, wParam, lParam))
                return swallowResult(msg);
            // Snapshot: restore() on the eject thread can null g_origWndProc mid message here
            // (mingw has no SEH to guard the window).
            const WNDPROC orig = g_origWndProc;
            if (orig == nullptr)
                return DefWindowProc(hwnd, msg, wParam, lParam);
            return CallWindowProc(orig, hwnd, msg, wParam, lParam);
        }
    }

    bool ensureHook(HWND hwnd, d3d9::WndProcFn onWndProc)
    {
        if (g_window != nullptr)
            return true;
        if (hwnd == nullptr)
            return false;

        g_onWndProc = onWndProc;
        g_window = hwnd;
        g_origWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&wndProc)));
        g_active.store(true);

        LOGC(Debug, kCategory, "WndProc subclassed on hwnd 0x%X", fmt::ptr(hwnd));
        return true;
    }

    void restore()
    {
        g_active.store(false);
        if (g_origWndProc != nullptr && g_window != nullptr)
            SetWindowLongPtrA(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_origWndProc));

        g_origWndProc = nullptr;
        g_window = nullptr;
        g_onWndProc = nullptr;
    }

    HWND window()
    {
        return g_window;
    }
}