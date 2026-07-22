#pragma once
// Loader-owned ImGui overlay: the ONE ImGui context, the DX9 + Win32 backends, the per-frame
// New/Render, DPI + user scaling, device-reset recreate, the toggle-key edge, and the game input
// freeze. Mods never touch any of this - they register a draw callback (see CubeOverlayApi /
// cube::Menu) and write widgets. Because the loader owns the single context, any number of mods can
// each draw their own menu. Subscribes to render_dispatch lazily on the first registered menu and
// stays subscribed until shutdown() (so backends are never re-initialised mid-session).

#include "cube_sdk.h"

#include <cstdint>

namespace modloader::overlay
{
    typedef void (CUBE_CALL* DrawFn)(void* user); // runs between the loader's NewFrame and Render

    // Register a menu owned by `owner` (attributes faults + scopes unregisterOwner). Returns a nonzero
    // handle, 0 on bad args. Arms the overlay (subscribes to render_dispatch) on the first menu.
    uint32_t registerMenu(const CubeApi* owner, DrawFn fn, void* user, uint32_t toggleKey, bool startOpen);
    // Remove one menu (must be owned by `owner`). Returns true if it existed.
    bool unregisterMenu(const CubeApi* owner, uint32_t handle);
    // Drop every menu owned by `owner` - the unload path.
    void unregisterOwner(const CubeApi* owner);

    // Per-menu visibility + toggle key + HUD passthrough. setVisible recomputes the aggregate input
    // freeze. Each returns false on an unknown handle.
    bool setVisible(uint32_t handle, bool visible);
    bool isVisible(uint32_t handle);
    bool setToggleKey(uint32_t handle, uint32_t vkey);
    bool setPassthrough(uint32_t handle, bool passthrough);
    bool passthrough(uint32_t handle);

    // Shared user UI scale (clamped [kMinUiScale, kMaxUiScale]) applied on top of the monitor DPI.
    void setUiScale(float scale);
    float uiScale();
    float dpiScale();

    constexpr float kMinUiScale = 0.5f;
    constexpr float kMaxUiScale = 3.0f;

    // Shared-context handoff for a mod that compiles its own ImGui (Strategy B). context() is the
    // loader's ImGuiContext* (NULL until the first frame inits it); allocFuncs() reports the ImGui
    // allocator the loader uses so the mod binds the same heap.
    void* context();
    void allocFuncs(void** allocFn, void** freeFn, void** userData);

    // Loader teardown: unsubscribe from render_dispatch (drains an in-flight frame), shut the backends
    // down and destroy the context. Called from lifecycle remove() before mods are torn down.
    void shutdown();
}
