#pragma once
// Generic capture pool for raw (user-address) hooks; __thiscall/__cdecl, up to CUBE_HOOK_ARG_MAX int/ptr args, int/ptr return.
#include "cube_sdk.h"

#include <cstdint>

namespace game::gamehooks::rawpool
{
    constexpr int kSlotMax = 24; // fixed slot count; onRaw fails (logged) when all are in use

    // Dispatches through gamehooks::dispatchRaw on the game thread.
    bool install(uint32_t address, CubeCallConv cc, int32_t argCount);
    // Returns true if one existed.
    bool remove(uint32_t address);
    // Reports the (cc, argCount) a live slot was installed with; false if no slot hooks address.
    bool config(uint32_t address, CubeCallConv& ccOut, int32_t& argCountOut);
    void shutdown();
}
