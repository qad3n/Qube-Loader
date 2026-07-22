#include "overlay/overlay.h"
#include "hooks/render_dispatch.h"
#include "hooks/d3d9_hook.h"
#include "hooks/input_block.h"
#include "core/log.h"
#include "util/guard.h"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include <windows.h>
#include <d3d9.h>

#include <atomic>
#include <mutex>
#include <string>
#include <vector>

// Forward declared per the imgui_impl_win32.h instructions (kept in a '#if 0' block there to avoid
// pulling <windows.h> into the header).
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace modloader::overlay
{
    namespace
    {
        constexpr char kCategory[] = "overlay";
        constexpr LPARAM kKeyRepeatMask = 0x40000000; // WM_KEYDOWN lParam bit 30: key already down
        constexpr double kMsPerSecond = 1000.0;
        constexpr double kMenuTargetFps = 60.0;
        constexpr double kMenuIntervalMs = kMsPerSecond / kMenuTargetFps;
        constexpr DWORD kDrainMs = 32; // let an in flight frame finish before an owner's code is freed
        constexpr int kMaxMenuFaults = 8; // disable a menu that keeps throwing, so it can't burn CPU/log

        // One registered menu. Owned by `owner` (a mod's CubeApi*) for fault attribution + unload scoping.
        struct Menu
        {
            uint32_t handle;
            const CubeApi* owner;
            DrawFn fn;
            void* user;
            uint32_t toggleKey;
            bool visible;
            bool passthrough;
            int faults; // cumulative draw faults; at kMaxMenuFaults the menu is disabled (fn cleared)
        };

        std::mutex g_mutex; // guards g_menus / g_nextHandle / g_token / g_armed
        std::vector<Menu> g_menus;
        uint32_t g_nextHandle = 1; // 0 is the invalid handle
        hooks::render::Token g_token = hooks::render::kInvalidToken;
        bool g_armed = false;

        // Render/window thread ImGui state.
        bool g_ready = false;
        std::atomic<bool> g_initFailed{false};
        bool g_haveDrawData = false;
        LARGE_INTEGER g_qpcFreq = {};
        LARGE_INTEGER g_lastBuild = {};

        ImGuiStyle g_baseStyle; // style snapshot at scale 1.0
        HWND g_hwnd = nullptr; // cached for DPI re query on device reset
        std::atomic<float> g_dpiScale{1.0f};
        std::atomic<float> g_uiScale{1.0f};
        std::atomic<bool> g_styleDirty{true};
        std::atomic<bool> g_inputBlocked{false}; // last value pushed to input_block (edge tracking)
        // Cached aggregates so the per message WndProc swallow check and the per frame resubmit gate are
        // O(1) atomic reads instead of a locked scan of every mod's menus. Refreshed by syncAggregates
        // on any visibility/passthrough change. g_inputBlocked doubles as "any interactive menu open".
        std::atomic<bool> g_anyVisible{false};

        // --- registry helpers (call under g_mutex) ---

        Menu* findLocked(uint32_t handle)
        {
            for (Menu& m : g_menus)
                if (m.handle == handle)
                    return &m;
            return nullptr;
        }

        // Refresh the cached aggregates (any visible / any interactive) from the current menu set and
        // push the input freeze on a change edge. Scans once under the lock, then calls the hook outside
        // it (never hold g_mutex across a hook call). Called on every visibility/passthrough change, so
        // the render + WndProc hot paths never scan the registry themselves.
        void syncAggregates()
        {
            bool anyVisible = false;
            bool anyInteractive = false;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                for (const Menu& m : g_menus)
                {
                    if (!m.visible || !m.fn) // fn == nullptr: fault disabled, never draws
                        continue;
                    anyVisible = true;
                    if (!m.passthrough)
                    {
                        anyInteractive = true;
                        break; // interactive implies visible; nothing more to learn
                    }
                }
            }
            g_anyVisible.store(anyVisible);
            if (g_inputBlocked.exchange(anyInteractive) != anyInteractive)
                hooks::input_block::setBlocked(anyInteractive);
        }

        // A menu's draw threw: bump its fault count and, past the cap, disable it (clear fn) so a
        // persistently broken menu cannot burn CPU or flood the log every frame. Owner CPU faults are
        // already quarantined by faultguard; this is the menu granularity net for the C++-exception /
        // repeat case, and keeps one bad mod from degrading everyone else's menus.
        void chargeFault(uint32_t handle)
        {
            bool disabled = false;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                Menu* m = findLocked(handle);
                if (!m || !m->fn)
                    return;
                if (++m->faults >= kMaxMenuFaults)
                {
                    m->fn = nullptr; // disabled: the snapshot + aggregate scans skip fn == nullptr
                    disabled = true;
                    LOGC(Warn, kCategory, "menu %u disabled after %d draw faults (owner mod broken)", handle, m->faults);
                }
            }
            // Refresh outside the lock: a disabled menu no longer counts as visible/interactive, so if it
            // was the only one open the render + input freeze state must fall back to idle.
            if (disabled)
                syncAggregates();
        }

        // --- scaling (render thread) ---

        // Rederives style + font scale from DPI * user scale. Must run outside a NewFrame/EndFrame pair;
        // reapplies from the scale 1.0 snapshot so it never compounds. Re queries the monitor DPI here so
        // a resolution/monitor change flagged by a device reset or WM_DPICHANGED is reflected.
        void applyScaleIfDirty()
        {
            if (!g_styleDirty.exchange(false))
                return;
            if (g_hwnd != nullptr)
            {
                const float dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(g_hwnd);
                g_dpiScale.store(dpi > 0.0f ? dpi : 1.0f);
            }
            const float eff = g_dpiScale.load() * g_uiScale.load();
            ImGuiStyle& style = ImGui::GetStyle();
            style = g_baseStyle;
            style.ScaleAllSizes(eff);
            ImGui::GetIO().FontGlobalScale = eff;
        }

        // Throttles the UI rebuild to kMenuTargetFps regardless of the game's frame rate; between
        // rebuilds we resubmit cached draw data so the overlay stays smooth.
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

        // --- ImGui lifecycle (render thread) ---

        bool initImGui(IDirect3DDevice9* device, HWND hwnd)
        {
            LOGC(Debug, kCategory, "initializing ImGui (first frame)");
            if (device == nullptr || hwnd == nullptr)
            {
                LOGC(Error, kCategory, "no device/hwnd; cannot init ImGui");
                return false;
            }

            IMGUI_CHECKVERSION();
            if (ImGui::CreateContext() == nullptr)
            {
                LOGC(Error, kCategory, "ImGui::CreateContext returned null");
                return false;
            }
            ImGuiIO& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;

            if (!ImGui_ImplWin32_Init(hwnd))
            {
                LOGC(Error, kCategory, "ImGui_ImplWin32_Init failed");
                ImGui::DestroyContext();
                return false;
            }
            if (!ImGui_ImplDX9_Init(device))
            {
                LOGC(Error, kCategory, "ImGui_ImplDX9_Init failed");
                ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext();
                return false;
            }

            ImGui::StyleColorsDark(); // plain default look
            g_baseStyle = ImGui::GetStyle(); // snapshot at scale 1.0
            g_hwnd = hwnd;
            const float dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
            g_dpiScale.store(dpi > 0.0f ? dpi : 1.0f);
            g_styleDirty.store(true);
            LOGC(Info, kCategory, "overlay ready (loader-owned ImGui context)");
            return true;
        }

        // Discrete input a visible interactive menu eats so clicks/keys/wheel do not leak to the game
        // (movement/camera are frozen separately by input_block). ImGui receives them first.
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

        // --- render_dispatch callbacks ---

        void CUBE_CALL onRender(IDirect3DDevice9* device)
        {
            if (!g_ready)
            {
                if (g_initFailed.load())
                    return;
                // First EndScene: the game is mid frame with a valid device, the right time to init ImGui.
                if (!initImGui(device, hooks::d3d9::window()))
                {
                    g_initFailed.store(true);
                    hooks::input_block::setBlocked(false); // never leave the game frozen with no menu
                    LOGC(Error, kCategory, "ImGui init failed; overlay disabled this session");
                    return;
                }
                g_ready = true;
            }

            // Draw with the OS cursor (input_block reveals it while a menu is open); a second ImGui
            // software cursor would just double it.
            ImGui::GetIO().MouseDrawCursor = false;

            // Nothing open: cheap O(1) atomic read (no lock, no scan), the common case. Drop any cached
            // draw data so a menu closed between rebuilds stops showing on the very next frame.
            if (!g_anyVisible.load())
            {
                g_haveDrawData = false;
                return;
            }

            // Only rebuild the UI at the throttle rate; between rebuilds we resubmit the cached draw data
            // (so a 300 fps game does not rebuild every menu 300 times a second). The snapshot + widget
            // work, the only per menu cost, happens solely on a rebuild frame.
            if (menuRebuildDue())
            {
                applyScaleIfDirty();

                // Snapshot the visible menus (a draw callback may (un)register mid iteration). Reuse one
                // render thread buffer so this is not a per frame heap allocation as menu count grows.
                static std::vector<Menu> visible;
                visible.clear();
                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    for (const Menu& m : g_menus)
                        if (m.visible && m.fn)
                            visible.push_back(m);
                }

                if (visible.empty())
                {
                    // Raced to hidden after the atomic read; skip cleanly (no unbalanced NewFrame).
                    g_haveDrawData = false;
                    return;
                }

                ImGui_ImplDX9_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();
                for (const Menu& m : visible)
                {
                    // Each menu's widget code is fault isolated + attributed to its owner mod, so one
                    // crashing menu is quarantined, not fatal to the game or the other menus. A repeat
                    // offender is disabled by chargeFault (rare path; the lock is brief and off the
                    // steady state).
                    if (!guard::tryRun("overlay draw", m.owner, [&]() { m.fn(m.user); }))
                        chargeFault(m.handle);
                }
                ImGui::EndFrame();
                ImGui::Render();
                g_haveDrawData = true;
            }

            if (g_haveDrawData)
                ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        }

        void CUBE_CALL onDeviceReset(bool preReset)
        {
            if (!g_ready)
                return;
            if (preReset)
            {
                ImGui_ImplDX9_InvalidateDeviceObjects();
            }
            else
            {
                ImGui_ImplDX9_CreateDeviceObjects();
                // Resolution/monitor may have changed; refit style + DPI on the next visible frame.
                g_styleDirty.store(true);
            }
        }

        bool CUBE_CALL onWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            // Monitor/DPI changed (dragged to another display): flag a refit; the render thread
            // re queries the DPI in applyScaleIfDirty, so no cross thread ImGui write here.
            if (msg == WM_DPICHANGED)
                g_styleDirty.store(true);

            // Focus loss (alt tab / Win+D / minimize): never leave the game with its DirectInput zeroed
            // or the cursor captured. Release the freeze while unfocused; on refocus reapply it if an
            // interactive menu is still open.
            if (msg == WM_ACTIVATEAPP && g_ready)
            {
                const bool focused = wParam != FALSE;
                if (!focused)
                {
                    // Force release and update the tracker so a later refocus re freezes correctly.
                    if (g_inputBlocked.exchange(false))
                        hooks::input_block::setBlocked(false);
                }
                else
                    syncAggregates();
            }

            // Toggle key edges: flip every menu whose key matches (ignore auto repeat). A menu with
            // toggleKey 0 has no key and is unaffected.
            bool toggled = false;
            if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && (lParam & kKeyRepeatMask) == 0 &&
                !g_initFailed.load())
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                for (Menu& m : g_menus)
                {
                    if (m.toggleKey != 0 && wParam == m.toggleKey)
                    {
                        m.visible = !m.visible;
                        toggled = true;
                    }
                }
            }
            if (toggled)
                syncAggregates();

            if (!g_ready)
                return false;

            ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);

            // While an interactive menu owns input, eat discrete input so it never reaches the game
            // (ImGui saw it above). In HUD passthrough the game keeps receiving it, so do not swallow.
            // g_inputBlocked is exactly "an interactive menu is open", an O(1) atomic read, so this runs
            // per window message (mouse move can be frequent) without locking or scanning the registry.
            return isBlockableInput(msg) && g_inputBlocked.load();
        }

    }

    uint32_t registerMenu(const CubeApi* owner, DrawFn fn, void* user, uint32_t toggleKey, bool startOpen)
    {
        if (!owner || !fn)
            return 0;
        uint32_t handle = 0;
        bool needArm = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (!g_armed)
            {
                g_armed = true;
                needArm = true; // first ever menu: arm the render dispatch outside the lock below
            }
            handle = g_nextHandle++;
            g_menus.push_back(Menu{handle, owner, fn, user, toggleKey, startOpen, false, 0});
        }

        // Subscribe outside g_mutex: render_dispatch::subscribe takes its OWN lock, and the render
        // callbacks reacquire g_mutex, so nesting the two here would invert lock order. The registry
        // already carries the menu, so a frame that starts before this returns simply draws nothing yet.
        if (needArm)
        {
            hooks::d3d9::Callbacks callbacks;
            callbacks.onRender = &onRender;
            callbacks.onDeviceReset = &onDeviceReset;
            callbacks.onWndProc = &onWndProc;
            const hooks::render::Token token = hooks::render::subscribe(callbacks);
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_token = token;
            }
            LOGC(Debug, kCategory, "armed (first menu registered)");
        }

        if (startOpen)
            syncAggregates();
        return handle;
    }

    bool unregisterMenu(const CubeApi* owner, uint32_t handle)
    {
        bool removed = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            for (size_t i = 0; i < g_menus.size(); ++i)
            {
                if (g_menus[i].handle == handle && g_menus[i].owner == owner)
                {
                    g_menus.erase(g_menus.begin() + static_cast<std::ptrdiff_t>(i));
                    removed = true;
                    break;
                }
            }
        }
        if (removed)
            syncAggregates();
        return removed;
    }

    void unregisterOwner(const CubeApi* owner)
    {
        bool removed = false;
        bool armed = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            armed = g_armed;
            for (size_t i = g_menus.size(); i > 0; --i)
            {
                if (g_menus[i - 1].owner == owner)
                {
                    g_menus.erase(g_menus.begin() + static_cast<std::ptrdiff_t>(i - 1));
                    removed = true;
                }
            }
        }
        if (removed)
        {
            syncAggregates();
            // Called only on the unload (loader) thread, never from a draw. A frame already dispatching
            // holds a snapshot of this owner's draw fn; drain it before the caller frees the mod's code.
            if (armed)
                Sleep(kDrainMs);
        }
    }

    bool setVisible(uint32_t handle, bool visible)
    {
        bool ok = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (Menu* m = findLocked(handle))
            {
                m->visible = visible;
                ok = true;
            }
        }
        if (ok)
            syncAggregates();
        return ok;
    }

    bool isVisible(uint32_t handle)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const Menu* m = findLocked(handle);
        return m && m->visible;
    }

    bool setToggleKey(uint32_t handle, uint32_t vkey)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (Menu* m = findLocked(handle))
        {
            m->toggleKey = vkey;
            return true;
        }
        return false;
    }

    bool setPassthrough(uint32_t handle, bool passthrough)
    {
        bool ok = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (Menu* m = findLocked(handle))
            {
                m->passthrough = passthrough;
                ok = true;
            }
        }
        if (ok)
            syncAggregates();
        return ok;
    }

    bool passthrough(uint32_t handle)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const Menu* m = findLocked(handle);
        return m && m->passthrough;
    }

    void setUiScale(float scale)
    {
        if (scale < kMinUiScale)
            scale = kMinUiScale;
        if (scale > kMaxUiScale)
            scale = kMaxUiScale;
        g_uiScale.store(scale);
        g_styleDirty.store(true);
    }

    float uiScale()
    {
        return g_uiScale.load();
    }

    float dpiScale()
    {
        return g_dpiScale.load();
    }

    void* context()
    {
        // Only valid after the first frame inits ImGui; GetCurrentContext() is the loader's single ctx.
        return g_ready ? static_cast<void*>(ImGui::GetCurrentContext()) : nullptr;
    }

    void allocFuncs(void** allocFn, void** freeFn, void** userData)
    {
        ImGuiMemAllocFunc a = nullptr;
        ImGuiMemFreeFunc f = nullptr;
        void* u = nullptr;
        ImGui::GetAllocatorFunctions(&a, &f, &u);
        if (allocFn)
            *allocFn = reinterpret_cast<void*>(a);
        if (freeFn)
            *freeFn = reinterpret_cast<void*>(f);
        if (userData)
            *userData = u;
    }

    void shutdown()
    {
        // Stop per frame delivery FIRST (drains an in flight frame) so no render callback runs while we
        // destroy the context, then release the freeze and tear ImGui down.
        hooks::render::Token token = hooks::render::kInvalidToken;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            token = g_token;
            g_token = hooks::render::kInvalidToken;
            g_armed = false;
            g_menus.clear();
        }
        if (token != hooks::render::kInvalidToken)
            hooks::render::unsubscribe(token);

        if (g_inputBlocked.exchange(false))
            hooks::input_block::setBlocked(false);

        if (g_ready)
        {
            ImGui_ImplDX9_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            g_ready = false;
            LOGC(Debug, kCategory, "shut down (context destroyed)");
        }
    }
}
