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
        std::string category;
        int32_t priority = 0; // mod-declared dispatch priority; higher runs last in every reduce
    };
}
