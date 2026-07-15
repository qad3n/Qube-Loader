#pragma once
// Mod-facing game-function hook subsystem; handlers run on the game thread in guard::tryRun.
#include "cube_sdk.h"

#include <cstdint>
#include <functional>
#include <string>

namespace game::gamehooks
{
    const char* hookName(CubeHook hook);

    // Comma-separated list of every hook (built-in + raw) a given owner holds, for the load report.
    std::string describeOwner(const CubeApi* owner);
    // Enumerate all subscriptions as (owner, hook-label) pairs; feeds the compatibility report's index.
    void forEachSubscription(const std::function<void(const CubeApi*, const char*)>& fn);

    // Loader-internal: reserve the IMPACT detour (CombatBehavior::vfunc_0) installed for the whole
    // session so the attack watcher samples every tick, independent of any mod subscription. The
    // reservation keeps the detour PASS-THROUGH (it never activates dispatch), so with no mod hooking
    // IMPACT the game runs vanilla. Coexists with the mod-facing IMPACT hook via the install refcount
    // (no double-hook). Released at shutdown.
    void armAttackWatch();

    // Loader-internal: reserve the CRIT_ROLL detour installed so the local player's crits are counted
    // even with no mod hooked. Like armAttackWatch, the reservation is PASS-THROUGH (returns the real
    // roll untouched); only a mod that hooks CRIT_ROLL can change it. Released at shutdown.
    void armCritCounter();

    // `owner` is the mod's CubeApi pointer; first subscriber arms the detour, last disarms.
    void subscribe(const CubeApi* owner, CubeHook hook, CubeHookFn fn, void* user);
    int32_t unsubscribeHook(const CubeApi* owner, CubeHook hook);
    void unsubscribeOwner(const CubeApi* owner);
    void clear();
    void shutdown();

    // installRaw wraps a generic capture stub; installRawDetour lets the mod supply its own detour.
    int32_t installRaw(const CubeApi* owner, uint32_t address, CubeCallConv cc, int32_t argCount,
                       CubeHookFn fn, void* user);
    int32_t installRawDetour(const CubeApi* owner, uint32_t address, void* detour, void** trampoline);
    int32_t removeRaw(const CubeApi* owner, uint32_t address);

    // Runtime (rebased) address of the game function a built-in hook targets; 0 if the hook has no Def.
    uint32_t builtinTarget(CubeHook hook);

    // Multi-mod reduce: cancel sticky OR, args chained, return last-writer-wins. Returns call.cancel.
    int32_t dispatchBuiltin(CubeHook hook, CubeHookCall& call);
    int32_t dispatchRaw(uint32_t address, CubeHookCall& call);
}
