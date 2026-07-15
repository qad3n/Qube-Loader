#pragma once
// The loader's inline-hook layer, backed by MinHook, so init/teardown lives in one place. Hooking
// is by function address: create() installs a trampoline + jmp at the target's prologue.

namespace hooks::detour
{
    bool init(); // idempotent; safe to call before every create()
    void shutdown(); // uninitialize MinHook once no hooks remain

    // Install and enable an inline hook. `original` receives the trampoline. False on error (logged).
    bool create(void* target, void* detourFn, void** original);

    // Disable and remove a hook previously installed on `target`.
    bool remove(void* target);
}