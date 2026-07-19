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

    // Whether this owner may perform game-state writes (declared Writes, or the unrestricted 0 default).
    inline bool ownerMayWrite(const CubeApi* owner)
    {
        const uint32_t caps = ownerCapabilities(owner);
        return caps == 0 || (caps & CUBE_CAP_WRITES) != 0;
    }

    // Marks capability bit as warned for this owner and returns true only the first time, so a denied
    // power logs one WARN per (mod, capability) instead of every call. Log-only state; a race is benign.
    inline bool ownerWarnOnce(const CubeApi* owner, uint32_t bit)
    {
        if (!owner)
            return false;
        ModContext* ctx = reinterpret_cast<ModContext*>(const_cast<CubeApi*>(owner));
        if ((ctx->warnedCaps & bit) != 0)
            return false;
        ctx->warnedCaps |= bit;
        return true;
    }
}