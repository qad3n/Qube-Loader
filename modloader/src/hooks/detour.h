#pragma once
// The loader's inline hook layer, backed by MinHook, so init/teardown lives in one place. Hooking
// is by function address: create() installs a trampoline + jmp at the target's prologue.

namespace hooks::detour
{
    void shutdown(); // uninitialize MinHook once no hooks remain

    // Install and enable an inline hook. `original` receives the trampoline. False on error (logged).
    // MinHook is initialized lazily on the first create(), so there is no separate init step.
    bool create(void* target, void* detourFn, void** original);

    // Restore the prologue; the trampoline stays alive for threads still inside the detour.
    bool disable(void* target);

    // Frees the trampoline. Releasing while a thread is still inside the detour jumps into freed
    // code and kills the game (no SEH on mingw): disable, drain (util/inflight.h), then release.
    bool release(void* target);

    // Coupled disable + release. Only where nothing can be in flight, i.e. install rollback.
    bool remove(void* target);
}