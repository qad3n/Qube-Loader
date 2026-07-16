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
        std::string category;    // log label (display name, disambiguated on id collision)
        std::string id;          // stable machine id (manifest id, or DLL stem fallback)
        std::string stem;        // DLL filename stem; the enable/disable + fault-strike registry key
        int32_t priority = 0; // mod-declared dispatch priority; higher runs last in every reduce
        // Dependency topological rank within one priority (deps::resolve sets it; default 0 = load
        // order). A dependency ranks below its dependents so it dispatches first at equal priority.
        int32_t dispatchOrder = 0;
    };
}
