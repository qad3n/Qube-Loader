#pragma once
// Per-mod context owned by the loader. CubeApi MUST stay the first member: the
// bridge recovers the ModContext by casting the api pointer it is handed.
#include "cube_sdk.h"
#include <string>

namespace modloader
{
    struct ModContext
    {
        CubeApi api;
        std::string category;    // log label (stable id, disambiguated as id#N on collision)
        std::string id;          // stable machine id (manifest id, or DLL stem fallback)
        std::string stem;        // DLL filename stem; the enable/disable + fault-strike registry key
        int32_t priority = 0; // mod-declared dispatch priority; higher runs last in every reduce
        // Dependency topological rank within one priority (deps::resolve sets it; default 0 = load
        // order). A dependency ranks below its dependents so it dispatches first at equal priority.
        int32_t dispatchOrder = 0;
        // Declared CubeModCapability bitset (0 = undeclared = unrestricted). Lives here (not on
        // LoadedMod) so the bridge recovers it from a CubeApi* to gate undeclared calls at call time.
        uint32_t capabilities = 0;
        // Per-capability "already warned once" bits, so a denied call logs one WARN per (mod,
        // capability) instead of every frame. Log-only, so a benign race across threads is fine.
        uint32_t warnedCaps = 0;
    };
}
