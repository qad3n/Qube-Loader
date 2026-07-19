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
        std::string version;                 // copied for dependency version compares (deps::resolve)
        uint32_t requiredAbi = 0;            // ABI the mod declared it was built against (0 = unspecified)
        std::vector<Dep> deps;
    };

    // The process-wide loaded-mod list (owned/defined by core.cpp).
    std::vector<std::unique_ptr<LoadedMod>>& loadedMods();

    // Discovery + loading (loader.cpp): scan the mods dir and load each valid DLL into loadedMods().
    void scan(const std::string& modsDir);

    // Drop every loader-side registration owned by a mod's CubeApi (events, game hooks, services +
    // message handlers). The single teardown step shared by loadOne's reject paths and unloadOne.
    void detachOwner(const CubeApi* api);

    // Fully unload one mod already in loadedMods(): guarded shutdown, detachOwner, FreeLibrary, then
    // erase it from the list (lifecycle.cpp). Used by remove() and by dependency-resolution cascade.
    void unloadOne(LoadedMod* mod);

    // Dependency resolution (deps.cpp): after scan and before READY, unload mods whose hard deps are
    // missing/underversioned (cascading), warn on missing soft deps, and topo-rank the dispatch order.
    void resolveDependencies();

    // Compatibility report (report.cpp): log what each mod attached to and warn on shared hooks.
    void reportCompatibility();
}
