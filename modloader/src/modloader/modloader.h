#pragma once
// Discovers and loads mod DLLs from <dllDir>/mods, hands each a CubeApi, and fans per-frame events
// to them via the render dispatch.

#include <cstddef>
#include <string>

namespace modloader
{
    // overlayEnabled=false is the safe-mode escape hatch: mods still load, but the D3D9 overlay +
    // input hooks are not installed (no render-driven events, no probe device). See config.overlay.
    std::size_t install(const std::string& dllDir, bool overlayEnabled);
    void remove();
}