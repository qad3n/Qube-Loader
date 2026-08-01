#pragma once
// Mod facing game function hook subsystem; handlers run on the game thread in guard::tryRun.
#include "cube_sdk.h"

#include <cstdint>
#include <functional>
#include <string>

namespace game::gamehooks
{
    const char* hookName(CubeHook hook);

    // Comma separated list of every hook (built in + raw) a given owner holds, for the load report.
    std::string describeOwner(const CubeApi* owner);
    // Enumerate all subscriptions as (owner, hook label) pairs; feeds the compatibility report's index.
    void forEachSubscription(const std::function<void(const CubeApi*, const char*)>& fn);

    // Loader internal: reserve a built in detour installed so the loader can OBSERVE what it carries
    // (crit rolls) without a mod hooking it. The reservation keeps the
    // detour PASS THROUGH: it never flips the dispatch gate, so the observed function runs vanilla and
    // only its side channel is read. It shares one refcount with mod subscriptions, so a detour is
    // never installed twice and stays armed until BOTH the last subscriber and the last observer are
    // gone. Returns whether the detour is installed afterwards.
    bool acquireObservation(CubeHook hook);
    void releaseObservation(CubeHook hook);

    // `owner` is the mod's CubeApi pointer; first subscriber arms the detour, last disarms.
    // 0 means the subscription was dropped (bad hook id, or the detour failed to arm).
    int32_t subscribe(const CubeApi* owner, CubeHook hook, CubeHookFn fn, void* user);
    int32_t unsubscribeHook(const CubeApi* owner, CubeHook hook);
    void unsubscribeOwner(const CubeApi* owner);
    void clear();
    void shutdown();

    // installRaw wraps a generic capture stub; installRawDetour lets the mod supply its own detour.
    int32_t installRaw(const CubeApi* owner, uint32_t address, CubeCallConv cc, int32_t argCount,
                       CubeHookFn fn, void* user);
    int32_t installRawDetour(const CubeApi* owner, uint32_t address, void* detour, void** trampoline);
    int32_t removeRaw(const CubeApi* owner, uint32_t address);

    // Runtime (rebased) address of the game function a built in hook targets; 0 if the hook has no Def.
    uint32_t builtinTarget(CubeHook hook);

    // Multi mod reduce: cancel sticky OR, args chained, return last writer wins. Returns call.cancel.
    int32_t dispatchBuiltin(CubeHook hook, CubeHookCall& call);
    int32_t dispatchRaw(uint32_t address, CubeHookCall& call);
}
