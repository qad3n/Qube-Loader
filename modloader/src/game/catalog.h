#pragma once
// Name catalogs mapping opaque game ids to display names.

#include "cube_sdk.h"
#include <cstdint>

namespace game
{
    // Name for id within catalog, or nullptr if unmapped (static string).
    const char* catalogName(int32_t catalog, int32_t id);
    // Fills up to maxCount (id,name) entries of catalog into out; returns the count.
    int32_t catalogList(int32_t catalog, CubeNamedValue* out, int32_t maxCount);
    // Writes catalog[id] name into out (size bytes), or "<prefix> #id" when the id is unmapped.
    void catalogNameOr(int32_t catalog, int32_t id, char* out, int32_t size, const char* prefix);
}
