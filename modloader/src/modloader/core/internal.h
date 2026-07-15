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

    // A copied dependency declaration (owned by the loader, decoupled from the mod DLL's statics).
    struct Dep
    {
        std::string id;
        std::string minVersion; // empty = any
        bool hard = true;
    };

    struct LoadedMod
    {
        HMODULE module = nullptr;
        ModContext context;
        CubeModShutdownFn shutdown = nullptr;
        std::string name;
        std::string version;                 // copied for dependency version compares (F8)
        uint32_t requiredAbi = 0;            // ABI the mod declared it was built against (0 = unspecified)
        uint32_t capabilities = 0;          // CubeModCapability bitset (0 = unrestricted)
        std::vector<Dep> deps;
    };

    // The process-wide loaded-mod list (owned/defined by core.cpp).
    std::vector<std::unique_ptr<LoadedMod>>& loadedMods();

    // Discovery + loading (loader.cpp): scan the mods dir and load each valid DLL into loadedMods().
    void scan(const std::string& modsDir);

    // Compatibility report (report.cpp): log what each mod attached to and warn on shared hooks.
    void reportCompatibility();
}
