#include "overlay/overlay.h"
#include "hooks/render_dispatch.h"
#include "hooks/d3d9_hook.h"
#include "hooks/input_block.h"
#include "core/log.h"
#include "loader/core/owner_name.h"
#include "util/guard.h"
#include "util/inflight.h"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include <windows.h>
#include <d3d9.h>

#include <atomic>
#include <mutex>
#include <vector>

// Forward declared per the imgui_impl_win32.h instructions (kept in a '#if 0' block there to avoid
// pulling <windows.h> into the header).
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

namespace modloader::overlay
{
    namespace
    {
        constexpr char kCategory[] = "overlay";
        constexpr LPARAM kKeyRepeatMask = 0x40000000; // WM_KEYDOWN lParam bit 30: key already down
        constexpr int kMaxMenuFaults = 8; // disable a menu that keeps throwing, so it can't burn CPU/log
        // ImGui's clocks (double click, key repeat, tooltip delay, animations) run off io.DeltaTime.
        // The Win32 backend derives it from wall clock, so the first frame after the menu reopens would
        // otherwise report the whole closed period as one frame. Clamp it to a sane worst case frame.
        constexpr float kMaxDeltaTime = 0.1f;
        constexpr float kFallbackDeltaTime = 1.0f / 60.0f;

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
        std::atomic<int> g_drawInFlight{0};
        std::atomic<bool> g_enabled{true};
        std::atomic<bool> g_shutdown{false};

        bool acceptingMenus(const CubeApi* owner)
        {
            if (!g_enabled.load(std::memory_order_acquire))
            {
                LOGC(Warn, kCategory, "'%s' menu refused: overlay disabled by config (overlay=0 safe mode)",
                     ownerName(owner));
                return false;
            }
            // Re-arming from a SHUTDOWN handler would reinstall the D3D9 hook and WndProc subclass
            // that nothing tears down again, leaving the window proc in unmapped memory after eject.
            if (g_shutdown.load(std::memory_order_acquire))
            {
                LOGC(Warn, kCategory, "'%s' menu refused: the overlay is already shut down",
                     ownerName(owner));
                return false;
            }
            return true;
        }
        std::vector<Menu> g_menus;
        uint32_t g_nextHandle = 1; // 0 is the invalid handle
        hooks::render::Token g_token = hooks::render::kInvalidToken;
        bool g_armed = false;

        // Render/window thread ImGui state.
        std::atomic<bool> g_ready{false};
        std::atomic<bool> g_initFailed{false};

        ImGuiStyle g_baseStyle; // style snapshot at scale 1.0
        HWND g_hwnd = nullptr; // cached for DPI re query on device reset
        std::atomic<float> g_dpiScale{1.0f};
        std::atomic<float> g_uiScale{1.0f};
        std::atomic<bool> g_styleDirty{true};
        std::atomic<bool> g_inputBlocked{false}; // last value pushed to input_block (edge tracking)
        // Cached aggregate so the per frame gate is an O(1) atomic read instead of a locked scan of
        // every mod's menus. Refreshed by syncAggregates on any visibility/passthrough change.
        // g_inputBlocked doubles as "any interactive menu open".
        std::atomic<bool> g_anyVisible{false};
        // Whether ImGui is currently being fed window messages. ImGui only drains its input event queue
        // inside NewFrame, and we only run NewFrame while a menu is visible, so feeding it while nothing
        // is open would grow g.InputEventsQueue for the whole session and then replay hours of stale
        // clicks/keys the moment the menu opens. Flipped by the render thread (which owns the ImGui
        // frame) on the visibility edge, read by the WndProc gate; the game pumps messages and presents
        // on one thread, so a message is never forwarded outside a fed period.
        std::atomic<bool> g_feedingImGui{false};
        // Mouse buttons ImGui currently believes are held. The Win32 backend grabs SetCapture on the
        // first button down and only drops it on the matching up, so a menu closed mid drag (or with a
        // button still held) would leave the game window captured and the backend out of sync.
        std::atomic<uint32_t> g_mouseDownMask{0};

        // --- registry helpers (call under g_mutex) ---

        Menu* findLocked(uint32_t handle)
        {
            for (Menu& m : g_menus)
                if (m.handle == handle)
                    return &m;
            return nullptr;
        }

        // Handles are sequential, so without the owner check a mod could drive another mod's menus
        // by guessing one.
        Menu* findOwnedLocked(const CubeApi* owner, uint32_t handle)
        {
            Menu* m = findLocked(handle);
            return (m && m->owner == owner) ? m : nullptr;
        }

        // Refresh the cached aggregates (any visible / any interactive) from the current menu set and
        // push the input freeze on a change edge. Scans once under the lock, then calls the hook outside
        // it (never hold g_mutex across a hook call). Called on every visibility/passthrough change, so
        // the render + WndProc hot paths never scan the registry themselves.
        void syncAggregates()
        {
            bool anyVisible = false;
            bool anyInteractive = false;
            // A failed ImGui init draws nothing, so treating menus as visible here would freeze the
            // game behind an overlay that can never appear or be toggled away.
            if (!g_initFailed.load())
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
                    LOGC(Warn, kCategory, "menu %u disabled after %d draw faults (owner mod broken)", handle,
                         m->faults);
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
            style.FontScaleMain = eff; // 1.92+ font scale knob (io.FontGlobalScale is the obsolete alias)
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
            // Draw with the OS cursor (input_block reveals it while a menu is open, and the Win32
            // backend sets its shape from WM_SETCURSOR); a second ImGui software cursor would double it.
            io.MouseDrawCursor = false;

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

        // Start/stop forwarding window messages to ImGui, and wipe the input state on both edges so a
        // menu never opens holding keys/buttons the user pressed while it was closed, and never leaves
        // ImGui thinking a key is still down after it closes. On open we also seed the current cursor
        // position, so a click that lands before the first WM_MOUSEMOVE hits the right widget.
        void setFeedingImGui(bool feeding)
        {
            if (g_feedingImGui.exchange(feeding) == feeding)
                return;

            ImGuiIO& io = ImGui::GetIO();
            io.ClearEventsQueue();
            io.ClearInputKeys();
            io.ClearInputMouse();

            // Hand the mouse back to the game: if the menu went away while ImGui still thought a button
            // was down, the backend's SetCapture was never matched by a ReleaseCapture. Release it only
            // when we are the ones still holding a button, so a capture the game took stays the game's.
            if (g_mouseDownMask.exchange(0) != 0 && GetCapture() == g_hwnd)
                ReleaseCapture();

            if (!feeding)
                return;

            POINT cursor = {};
            if (g_hwnd != nullptr && GetCursorPos(&cursor) && ScreenToClient(g_hwnd, &cursor))
                io.AddMousePosEvent(static_cast<float>(cursor.x), static_cast<float>(cursor.y));
        }

        // Mirror the Win32 backend's button bookkeeping for the messages we forward, so the close edge
        // knows whether ImGui is still holding a capture. 0 for messages that are not a button edge.
        uint32_t mouseButtonBit(UINT msg, WPARAM wParam)
        {
            switch (msg)
            {
                case WM_LBUTTONDOWN:
                case WM_LBUTTONDBLCLK:
                case WM_LBUTTONUP:
                    return 1u << 0;
                case WM_RBUTTONDOWN:
                case WM_RBUTTONDBLCLK:
                case WM_RBUTTONUP:
                    return 1u << 1;
                case WM_MBUTTONDOWN:
                case WM_MBUTTONDBLCLK:
                case WM_MBUTTONUP:
                    return 1u << 2;
                case WM_XBUTTONDOWN:
                case WM_XBUTTONDBLCLK:
                case WM_XBUTTONUP:
                    return (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) ? (1u << 3) : (1u << 4);
                default:
                    return 0;
            }
        }

        bool isMouseButtonDown(UINT msg)
        {
            switch (msg)
            {
                case WM_LBUTTONDOWN:
                case WM_LBUTTONDBLCLK:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONDBLCLK:
                case WM_MBUTTONDOWN:
                case WM_MBUTTONDBLCLK:
                case WM_XBUTTONDOWN:
                case WM_XBUTTONDBLCLK:
                    return true;
                default:
                    return false;
            }
        }

        // --- render_dispatch callbacks ---

        void CUBE_CALL onRender(IDirect3DDevice9* device)
        {
            barrier::InFlight inflight(g_drawInFlight);
            if (!g_ready.load())
            {
                if (g_initFailed.load())
                    return;
                // First EndScene: the game is mid frame with a valid device, the right time to init ImGui.
                if (!initImGui(device, hooks::d3d9::window()))
                {
                    g_initFailed.store(true);
                    // Through the edge tracker, or it stays stale and the next edge misfires.
                    if (g_inputBlocked.exchange(false))
                        hooks::input_block::setBlocked(false);
                    LOGC(Error, kCategory, "ImGui init failed; overlay disabled this session");
                    return;
                }
                g_ready.store(true);
            }

            // Nothing open: cheap O(1) atomic read (no lock, no scan), the common case.
            if (!g_anyVisible.load())
            {
                setFeedingImGui(false);
                return;
            }

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
                setFeedingImGui(false);
                return;
            }

            setFeedingImGui(true);
            applyScaleIfDirty(); // must run outside the NewFrame/EndFrame pair (it rewrites the style)

            // One full ImGui frame per rendered frame while a menu is open. This is deliberately NOT
            // throttled: ImGui drains its queued window messages here and only here, so building fewer
            // frames than the game pumps messages would back the queue up and lag every click behind a
            // growing trail of stale input.
            ImGui_ImplDX9_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGuiIO& io = ImGui::GetIO();
            if (io.DeltaTime > kMaxDeltaTime || io.DeltaTime <= 0.0f)
                io.DeltaTime = (io.DeltaTime > 0.0f) ? kMaxDeltaTime : kFallbackDeltaTime;
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

            if (ImDrawData* drawData = ImGui::GetDrawData())
                ImGui_ImplDX9_RenderDrawData(drawData);
        }

        void CUBE_CALL onDeviceReset(bool preReset)
        {
            if (!g_ready.load())
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
            if (msg == WM_ACTIVATEAPP && g_ready.load())
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

            // Feed ImGui ONLY while a menu is on screen. Its event queue is drained in NewFrame, which
            // only runs while one is open, so forwarding messages the rest of the time would queue every
            // mouse move and key press of the whole session and replay them the moment the menu opens.
            if (!g_ready.load() || !g_feedingImGui.load())
                return false;

            const LRESULT handled = ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);

            if (const uint32_t bit = mouseButtonBit(msg, wParam))
            {
                if (isMouseButtonDown(msg))
                    g_mouseDownMask.fetch_or(bit);
                else
                    g_mouseDownMask.fetch_and(~bit);
            }

            // While an interactive menu owns input, eat discrete input so it never reaches the game
            // (ImGui saw it above), plus anything the backend claimed outright: that is WM_SETCURSOR,
            // where letting the game's own handler run would hide the pointer under the open menu. In
            // HUD passthrough the game keeps receiving everything, so do not swallow. g_inputBlocked is
            // exactly "an interactive menu is open", an O(1) atomic read, so this runs per window
            // message (mouse move can be frequent) without locking or scanning the registry.
            return (handled != 0 || isBlockableInput(msg)) && g_inputBlocked.load();
        }

    }

    void setEnabled(bool enabled)
    {
        g_enabled.store(enabled, std::memory_order_release);
    }

    uint32_t registerMenu(const CubeApi* owner, DrawFn fn, void* user, uint32_t toggleKey, bool startOpen)
    {
        if (!owner || !fn)
            return 0;

        if (!acceptingMenus(owner))
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
            if (token == hooks::render::kInvalidToken)
            {
                // No frame will ever run, so nothing would clear an input freeze this menu arms.
                std::lock_guard<std::mutex> lock(g_mutex);
                g_armed = false;
                for (size_t i = g_menus.size(); i > 0; --i)
                {
                    if (g_menus[i - 1].handle == handle)
                        g_menus.erase(g_menus.begin() + static_cast<std::ptrdiff_t>(i - 1));
                }
                LOGC(Error, kCategory, "'%s' menu refused: the D3D9 render dispatch failed to arm",
                     ownerName(owner));
                return 0;
            }
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
            // A dispatching frame holds a snapshot of this owner's draw fn and the caller frees its
            // code the moment we return.
            if (armed)
                barrier::drain(g_drawInFlight, "overlay draw");
        }
    }

    bool setVisible(const CubeApi* owner, uint32_t handle, bool visible)
    {
        bool ok = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (Menu* m = findOwnedLocked(owner, handle))
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

    bool setToggleKey(const CubeApi* owner, uint32_t handle, uint32_t vkey)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (Menu* m = findOwnedLocked(owner, handle))
        {
            m->toggleKey = vkey;
            return true;
        }
        return false;
    }

    bool setPassthrough(const CubeApi* owner, uint32_t handle, bool passthrough)
    {
        bool ok = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (Menu* m = findOwnedLocked(owner, handle))
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
        return g_ready.load() ? static_cast<void*>(ImGui::GetCurrentContext()) : nullptr;
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
        g_shutdown.store(true, std::memory_order_release);

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

        if (g_ready.exchange(false))
        {
            g_feedingImGui.store(false); // the WndProc gate must close before the context goes away
            g_anyVisible.store(false);
            ImGui_ImplDX9_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            LOGC(Debug, kCategory, "shut down (context destroyed)");
        }
    }
}
