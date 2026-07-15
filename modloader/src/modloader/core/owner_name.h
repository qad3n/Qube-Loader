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

    // Mod-declared dispatch priority (0 for the loader's internal sentinel owners, which have a
    // zeroed ModContext). Higher priority is dispatched last so it wins last-writer-wins reduces.
    inline int32_t ownerPriority(const CubeApi* owner)
    {
        if (!owner)
            return 0;
        return reinterpret_cast<const ModContext*>(owner)->priority;
    }
}