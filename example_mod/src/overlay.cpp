#include "overlay.h"
#include "menu/menu.h"
#include "mod_context.h"

#include "cube_mod.hpp"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include <windows.h>
#include <d3d9.h>
#include <atomic>

// Forward declared per the imgui_impl_win32.h instructions (kept in a '#if 0'
// block there to avoid pulling <windows.h> into the header).
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace exmod::overlay
{
    namespace
    {

        constexpr LPARAM kKeyRepeatMask = 0x40000000; // WM_KEYDOWN lParam bit 30: key already down
        constexpr double kMsPerSecond = 1000.0;
        constexpr double kMenuTargetFps = 60.0;
        constexpr double kMenuIntervalMs = kMsPerSecond / kMenuTargetFps;

        std::atomic<bool> g_visible{false};
        std::atomic<bool> g_focusRequest{false};
        std::atomic<bool> g_initFailed{false};
        bool g_ready = false; // render thread only
        bool g_haveDrawData = false; // render thread only
        LARGE_INTEGER g_qpcFreq = {};
        LARGE_INTEGER g_lastBuild = {};

        ImGuiStyle g_baseStyle; // style snapshot at scale 1.0 (render thread only)
        float g_dpiScale = 1.0f;
        float g_uiScale = 1.0f;
        bool g_styleDirty = true;

        void setInputBlocked(bool blocked)
        {
            cube::mod().setInputBlocked(blocked);
        }

        // Discrete input the menu eats so clicks/keys/wheel do not leak to the game (movement/camera
        // are frozen separately by the loader). ImGui receives them first, before we swallow.
        bool isBlockableInput(UINT msg)
        {
            switch (msg)
            {
                case WM_KEYDOWN:
                case WM_KEYUP:
                case WM_SYSKEYDOWN:
                case WM_SYSKEYUP:
                case WM_CHAR:
                case WM_SYSCHAR:
                case WM_MOUSEMOVE:
                case WM_LBUTTONDOWN:
                case WM_LBUTTONUP:
                case WM_LBUTTONDBLCLK:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP:
                case WM_RBUTTONDBLCLK:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONUP:
                case WM_MBUTTONDBLCLK:
                case WM_XBUTTONDOWN:
                case WM_XBUTTONUP:
                case WM_MOUSEWHEEL:
                case WM_MOUSEHWHEEL:
                    return true;
                default:
                    return false;
            }
        }

        // Throttles the UI rebuild to kMenuTargetFps regardless of the game's frame rate;
        // between rebuilds we resubmit cached draw data so the overlay stays smooth.
        bool menuRebuildDue()
        {
            if (g_qpcFreq.QuadPart == 0)
                QueryPerformanceFrequency(&g_qpcFreq);
            LARGE_INTEGER now = {};
            QueryPerformanceCounter(&now);
            const double elapsedMs =
                static_cast<double>(now.QuadPart - g_lastBuild.QuadPart) * kMsPerSecond /
                static_cast<double>(g_qpcFreq.QuadPart);
            if (elapsedMs < kMenuIntervalMs)
                return false;
            g_lastBuild = now;
            return true;
        }

        // Rederives style + font scale from DPI * user scale. Must run outside a
        // NewFrame/EndFrame pair; reapplies from the scale-1.0 snapshot so it never compounds.
        void applyScaleIfDirty()
        {
            if (!g_styleDirty)
                return;
            const float eff = g_dpiScale * g_uiScale;
            ImGuiStyle& style = ImGui::GetStyle();
            style = g_baseStyle;
            style.ScaleAllSizes(eff);
            ImGui::GetIO().FontGlobalScale = eff;
            g_styleDirty = false;
        }

        bool initImGui(IDirect3DDevice9* device, HWND hwnd)
        {
            logLine(CUBE_LOG_DEBUG, "example_mod: initializing ImGui (first frame)");
            if (device == nullptr || hwnd == nullptr)
            {
                logLine(CUBE_LOG_ERROR, "example_mod: no device/hwnd from the loader; cannot init ImGui");
                return false;
            }

            IMGUI_CHECKVERSION();
            if (ImGui::CreateContext() == nullptr)
            {
                logLine(CUBE_LOG_ERROR, "example_mod: ImGui::CreateContext returned null");
                return false;
            }
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;

            if (!ImGui_ImplWin32_Init(hwnd))
            {
                logLine(CUBE_LOG_ERROR, "example_mod: ImGui_ImplWin32_Init failed");
                ImGui::DestroyContext();
                return false;
            }
            if (!ImGui_ImplDX9_Init(device))
            {
                logLine(CUBE_LOG_ERROR, "example_mod: ImGui_ImplDX9_Init failed");
                ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext();
                return false;
            }

            ImGui::StyleColorsDark(); // plain default look (no extra theming/rounding)
            g_baseStyle = ImGui::GetStyle(); // snapshot at scale 1.0
            g_dpiScale = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
            if (g_dpiScale <= 0.0f)
                g_dpiScale = 1.0f;
            g_styleDirty = true;
            logLine(CUBE_LOG_INFO, "example_mod: menu ready. INSERT or DELETE toggles it.");
            return true;
        }

    }

    void setUiScale(float scale)
    {
        if (scale < kMinUiScale)
            scale = kMinUiScale;
        if (scale > kMaxUiScale)
            scale = kMaxUiScale;
        g_uiScale = scale;
        g_styleDirty = true;
    }

    float uiScale()
    {
        return g_uiScale;
    }

    float dpiScale()
    {
        return g_dpiScale;
    }

    void CUBE_CALL onFrame(CubeEventArgs* args)
    {
        if (!g_ready)
        {
            if (g_initFailed.load())
                return;
            if (!initImGui(static_cast<IDirect3DDevice9*>(args->device), static_cast<HWND>(args->hwnd)))
            {
                g_initFailed.store(true);
                setInputBlocked(false); // never leave the game frozen with no menu to close
                g_visible.store(false);
                logLine(CUBE_LOG_ERROR, "example_mod: ImGui init failed; menu disabled this session");
                return;
            }
            g_ready = true;
        }

        const bool visible = g_visible.load();
        // Rely on the OS cursor (the loader forces it visible while the menu is open). wine:
        // an ImGui software cursor too produced a second frozen cursor, so keep it off.
        ImGui::GetIO().MouseDrawCursor = false;
        if (!visible)
            return;

        applyScaleIfDirty();

        if (menuRebuildDue())
        {
            ImGui_ImplDX9_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            if (g_focusRequest.exchange(false))
                ImGui::SetNextWindowFocus();
            menu::draw(args);
            ImGui::EndFrame();
            ImGui::Render();
            g_haveDrawData = true;
        }

        if (g_haveDrawData)
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }

    void CUBE_CALL onDeviceReset(CubeEventArgs* args)
    {
        if (!g_ready)
            return;
        if (args->preReset)
            ImGui_ImplDX9_InvalidateDeviceObjects();
        else
            ImGui_ImplDX9_CreateDeviceObjects();
    }

    void CUBE_CALL onWndProc(CubeEventArgs* args)
    {
        HWND hwnd = static_cast<HWND>(args->hwnd);
        const UINT msg = args->msg;
        const WPARAM wParam = args->wParam;
        const LPARAM lParam = args->lParam;

        if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) &&
            (wParam == VK_INSERT || wParam == VK_DELETE) && (lParam & kKeyRepeatMask) == 0 &&
            !g_initFailed.load()) // never toggle (and freeze input) when the menu can't render
        {
            const bool nowVisible = !g_visible.load();
            g_visible.store(nowVisible);
            setInputBlocked(nowVisible); // freeze movement/camera/cursor while the menu is up
            if (nowVisible)
                g_focusRequest.store(true);
            cubeLogf(g_api, CUBE_LOG_INFO, "example_mod: menu %s", nowVisible ? "shown" : "hidden");
        }

        if (!g_ready)
            return;

        ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
        // While the menu is open, eat discrete input so it never reaches the game (ImGui
        // saw it above). Movement/camera are already frozen by the loader input hook.
        if (g_visible.load() && isBlockableInput(msg))
            args->swallow = 1;
    }

    void CUBE_CALL onShutdown(CubeEventArgs*)
    {
        setInputBlocked(false); // restore game input on unload
        if (g_ready)
        {
            ImGui_ImplDX9_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            g_ready = false;
        }
        logLine(CUBE_LOG_INFO, "example_mod: shutdown");
    }

}
