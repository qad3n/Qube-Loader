#pragma once
// The ImGui overlay layer of the SDK: cube::Menu + mod.menu(). This is the WHOLE surface a mod needs
// to draw a menu - register a draw callback and write ImGui code inside it. Everything else (the D3D9
// hook, the ImGui context, the DX9/Win32 backends, NewFrame/Render, the toggle key, DPI + scaling,
// device-reset recreate, the game input freeze, shutdown) lives in the loader (see CubeOverlayApi).
//
//   CUBE_MOD("My Mod", "1.0.0", "me")
//   {
//       mod.setCapabilities(cube::Capability::Overlay);
//       mod.menu().window("My Mod", []
//       {
//           ImGui::Text("Hello from my mod");
//           static bool god = false;
//           ImGui::Checkbox("God mode", &god);
//       });
//   }
//
// That is the entire mod. INSERT toggles the menu; the loader freezes the game while it is open. This
// header is opt-in: it pulls in imgui.h, so cube_mod.hpp only auto-includes it when the mod builds
// with ImGui on its include path (the SDK's cube_add_imgui CMake helper puts it there). A mod that
// draws no menu never includes this and never sees ImGui.
//
// Because the loader owns the ONE context, this wrapper binds the mod's own ImGui to it once (via the
// CubeOverlayApi context + allocator handoff) before the first draw - so ImGui:: calls in the mod land
// in the loader's live context and heap. The mod must build the SAME ImGui the loader ships (the
// modloader/sdk/imgui submodule, via cube_add_imgui) so the shared context is layout-compatible.

#include "cube/mod.hpp"

#include "imgui.h"

#include <cstdint>
#include <deque>
#include <functional>

namespace cube
{
    // A loader-drawn overlay menu owned by one mod. Register a draw callback with window()/onDraw();
    // the rest (toggle key, visibility, HUD passthrough, UI scale) is optional configuration. Every
    // setter returns *this so calls chain. All state is auto-released when the mod unloads.
    class Menu
    {
    public:
        static constexpr unsigned kDefaultToggleKey = 0x2D; // VK_INSERT
        // The loader clamps setUiScale() to this range; exposed so a scale slider can match the bounds.
        static constexpr float kMinUiScale = 0.5f;
        static constexpr float kMaxUiScale = 3.0f;

        explicit Menu(Mod* mod)
            : m_api(mod ? mod->raw() : nullptr) {}

        // Sugar: the loader wraps your widgets in ImGui::Begin(title)/End - you write ONLY the widgets
        // inside fn. The window is toggled by the toggle key (INSERT by default). Call again to replace fn.
        Menu& window(const char* title, std::function<void()> fn)
        {
            m_title = (title && title[0]) ? title : "Menu";
            m_draw = std::move(fn);
            m_wrapWindow = true;
            ensureRegistered();
            return *this;
        }

        // Raw draw callback: you own ImGui::Begin/End and may draw any number of windows / widgets. Runs
        // once per rendered frame while the menu is visible, with the loader's context already bound.
        Menu& onDraw(std::function<void()> fn)
        {
            m_draw = std::move(fn);
            m_wrapWindow = false;
            ensureRegistered();
            return *this;
        }

        // The VK_* key that toggles this menu's visibility (0 = no toggle, always drawn). Default INSERT.
        Menu& setToggleKey(unsigned vkey)
        {
            m_toggleKey = vkey;
            if (m_handle && m_api)
                m_api->overlay.setToggleKey(m_api, m_handle, vkey);
            return *this;
        }

        // Show/hide the menu programmatically (independent of the toggle key). Before the first
        // window()/onDraw() this only sets the initial visibility used at registration.
        Menu& setOpen(bool open)
        {
            m_startOpen = open;
            if (m_handle && m_api)
                m_api->overlay.setVisible(m_api, m_handle, open ? 1 : 0);
            return *this;
        }

        bool isOpen() const
        {
            return m_handle && m_api && m_api->overlay.isVisible(m_api, m_handle) != 0;
        }

        // HUD passthrough: when true, an open menu does NOT freeze the game (movement/camera stay live,
        // the game grabs the cursor so widgets are display-only). Default false (interactive: the menu
        // owns input while open).
        Menu& setPassthrough(bool passthrough)
        {
            if (m_handle && m_api)
                m_api->overlay.setPassthrough(m_api, m_handle, passthrough ? 1 : 0);
            return *this;
        }

        bool passthrough() const
        {
            return m_handle && m_api && m_api->overlay.passthrough(m_api, m_handle) != 0;
        }

        // Shared user UI scale (loader-global, since there is one context). Clamped [0.5, 3.0] by the
        // loader and applied on the next frame; multiplies on top of the monitor DPI.
        void setUiScale(float scale) const { if (m_api) m_api->overlay.setUiScale(m_api, scale); }
        float uiScale() const { return m_api ? m_api->overlay.uiScale(m_api) : 1.0f; }
        float dpiScale() const { return m_api ? m_api->overlay.dpiScale(m_api) : 1.0f; }

        // Effective scale for MANUAL pixel sizing (e.g. SetNextItemWidth): dpi * user scale. The loader
        // already scales ImGui's built-in style + font, so use this only for explicit pixel dimensions.
        float scale(float px) const { return px * uiScale() * dpiScale(); }

        // The loader-side registration handle (0 until the first window()/onDraw()). Rarely needed.
        unsigned handle() const { return m_handle; }

    private:
        void ensureRegistered()
        {
            if (m_handle || !m_api)
                return;
            m_handle = m_api->overlay.registerMenu(m_api, &trampoline, this, m_toggleKey, m_startOpen ? 1 : 0);
        }

        // The loader calls this each visible frame between its NewFrame and Render. Bind the shared
        // context once, then run the mod's draw (optionally wrapped in a Begin/End window).
        static void CUBE_CALL trampoline(void* user)
        {
            Menu* self = static_cast<Menu*>(user);
            if (!self)
                return;
            self->bindContextOnce();
            if (!self->m_draw)
                return;
            if (self->m_wrapWindow)
            {
                if (ImGui::Begin(self->m_title))
                    self->m_draw();
                ImGui::End();
            }
            else
            {
                self->m_draw();
            }
        }

        // Point this mod's ImGui globals (context + allocator) at the loader's, once. GImGui and the
        // allocator are per-DLL globals, so a mod that compiled its own ImGui must adopt the loader's or
        // its ImGui:: calls would target an empty context / a different heap.
        void bindContextOnce()
        {
            if (m_bound || !m_api)
                return;
            ImGuiContext* ctx = static_cast<ImGuiContext*>(m_api->overlay.context(m_api));
            if (!ctx)
                return; // overlay not initialised yet; retry next frame
            void* allocFn = nullptr;
            void* freeFn = nullptr;
            void* userData = nullptr;
            m_api->overlay.allocFuncs(m_api, &allocFn, &freeFn, &userData);
            ImGui::SetCurrentContext(ctx);
            if (allocFn && freeFn)
                ImGui::SetAllocatorFunctions(reinterpret_cast<ImGuiMemAllocFunc>(allocFn),
                                             reinterpret_cast<ImGuiMemFreeFunc>(freeFn), userData);
            m_bound = true;
        }

        const CubeApi* m_api = nullptr;
        std::function<void()> m_draw;
        const char* m_title = "Menu";
        bool m_wrapWindow = false;
        unsigned m_toggleKey = kDefaultToggleKey;
        bool m_startOpen = false;
        bool m_bound = false;
        uint32_t m_handle = 0;
    };

    namespace detail
    {
        // Process-lifetime storage for every Menu a mod creates. A deque never relocates its elements on
        // growth, so the Menu& handed back stays valid forever (unlike a vector). One shared pool is fine:
        // the SDK is one Mod singleton per DLL, and menus live until the DLL unloads.
        inline std::deque<Menu>& menuPool()
        {
            static std::deque<Menu> pool;
            return pool;
        }
    }

    // Each call creates a fresh, independently-toggled menu bound to this mod.
    inline Menu& Mod::addMenu()
    {
        detail::menuPool().emplace_back(this);
        return detail::menuPool().back();
    }

    // The default menu: the first one created for this mod, cached so every menu() call returns it.
    inline Menu& Mod::menu()
    {
        static Menu& instance = addMenu();
        return instance;
    }
}
