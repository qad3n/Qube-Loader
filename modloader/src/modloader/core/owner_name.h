#pragma once
// Recovers a mod's log name from its CubeApi pointer. CubeApi is the first member of the owning
// ModContext (api/context.h), so the api pointer reinterpret_casts straight back to the context.
#include "cube_sdk.h"
#include "api/context.h"

namespace modloader
{
    constexpr char kUnknownOwner[] = "?";

    inline const char* ownerName(const CubeApi* owner)
    {
        if (!owner)
            return kUnknownOwner;
        return reinterpret_cast<const ModContext*>(owner)->category.c_str();
    }

    // Stable machine id (manifest id, or DLL stem fallback) - the key for config/storage/services/deps.
    inline const char* ownerId(const CubeApi* owner)
    {
        if (!owner)
            return kUnknownOwner;
        return reinterpret_cast<const ModContext*>(owner)->id.c_str();
    }

    // DLL filename stem - the key for the enable/disable + fault-strike registry.
    inline const char* ownerStem(const CubeApi* owner)
    {
        if (!owner)
            return kUnknownOwner;
        return reinterpret_cast<const ModContext*>(owner)->stem.c_str();
    }

    // Mod-declared dispatch priority (0 for the loader's internal sentinel owners, which have a
    // zeroed ModContext). Higher priority is dispatched last so it wins last-writer-wins reduces.
    inline int32_t ownerPriority(const CubeApi* owner)
    {
        if (!owner)
            return 0;
        return reinterpret_cast<const ModContext*>(owner)->priority;
    }

    // Dependency topological rank (0 for the loader's internal sentinel owners). The secondary reduce
    // sort key behind priority: at equal priority a dependency (lower rank) dispatches before its
    // dependents. Set by deps::resolve; 0 when no dep graph reordering applies.
    inline int32_t ownerOrder(const CubeApi* owner)
    {
        if (!owner)
            return 0;
        return reinterpret_cast<const ModContext*>(owner)->dispatchOrder;
    }

    // Declared CubeModCapability bitset (0 = undeclared = unrestricted, and 0 for the loader's
    // internal sentinel owners). The bridge gates undeclared calls against this at call time.
    inline uint32_t ownerCapabilities(const CubeApi* owner)
    {
        if (!owner)
            return 0;
        return reinterpret_cast<const ModContext*>(owner)->capabilities;
    }
}