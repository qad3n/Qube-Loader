#pragma once
// Discovers and loads mod DLLs from <dllDir>/mods, hands each a CubeApi, and fans per-frame events
// to them via the render dispatch.

#include <cstddef>
#include <string>

namespace modloader
{
    std::size_t install(const std::string& dllDir);
    void remove();
}