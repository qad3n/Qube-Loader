#pragma once
// The loader's inline hook layer, backed by MinHook, so init/teardown lives in one place. Hooking
// is by function address: create() installs a trampoline + jmp at the target's prologue.

namespace hooks::detour
{
    void shutdown(); // uninitialize MinHook once no hooks remain

    // Install and enable an inline hook. `original` receives the trampoline. False on error (logged).
    // MinHook is initialized lazily on the first create(), so there is no separate init step.
    bool create(void* target, void* detourFn, void** original);

    // Disable and remove a hook previously installed on `target`.
    bool remove(void* target);
}