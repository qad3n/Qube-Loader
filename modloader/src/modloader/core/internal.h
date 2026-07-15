#pragma once
// Loader-internal shared surface (not part of the public modloader.h API): the loaded-mod list plus
// the discovery and report units that operate on it. Split across core.cpp / loader.cpp / report.cpp.

#include "api/context.h"
#include "cube_sdk.h"

#include <memory>
#include <string>
#include <vector>
#include <windows.h>

namespace modloader
{
    constexpr char kCategory[] = "modloader"; // shared log category for the loader units

    struct LoadedMod
    {
        HMODULE module = nullptr;
        ModContext context;
        CubeModShutdownFn shutdown = nullptr;
        std::string name;
    };

    // The process-wide loaded-mod list (owned/defined by core.cpp).
    std::vector<std::unique_ptr<LoadedMod>>& loadedMods();

    // Discovery + loading (loader.cpp): scan the mods dir and load each valid DLL into loadedMods().
    void scan(const std::string& modsDir);

    // Compatibility report (report.cpp): log what each mod attached to and warn on shared hooks.
    void reportCompatibility();
}
