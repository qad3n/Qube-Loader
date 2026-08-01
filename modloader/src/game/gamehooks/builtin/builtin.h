#pragma once
#include "cube_sdk.h"

#include <atomic>
#include <cstdint>

namespace game::gamehooks
{
    // Arm the detour backing `hook` (installs the trampoline; leaves it pass through). False if it
    // could not be hooked (logged).
    bool armBuiltin(CubeHook hook);
    // Set the dispatch gate: on = run subscribed handlers, off = pass through (run the original
    // untouched). Independent of install state; the detour stays armed either way.
    void setBuiltinActive(CubeHook hook, bool on);
    // Disarm the detour backing `hook` (last holder), with a short drain for an in flight call.
    void disarmBuiltin(CubeHook hook);
    // Complete any disarm that was deferred because it was requested from inside its own detour.
    void flushPendingDisarms();
    // Remove any still installed built in detour (loader teardown sweep).
    void shutdownBuiltin();

    namespace builtin
    {
        // One built in detour's registration record; each builtin/<hook>.cpp registers exactly one.
        struct Def
        {
            CubeHook hook;
            uintptr_t target; // static address of the game function (rebased at arm time)
            void* detour;
            void** original; // trampoline slot the detour calls through
            std::atomic<bool>* active; // gate: false = pass through, true = dispatch to handlers
        };

        // Register a detour definition; called once at static init from each builtin/<hook>.cpp.
        void registerDef(const Def& def);
        // Per hook in flight counter, bumped around each detour body so disarm can drain it.
        std::atomic<int>& inFlight(CubeHook hook);

        // Scoped marker for a detour body: bumps inFlight and records that THIS thread is inside the
        // detour. A handler that detaches its own hook would otherwise drain against its own frame,
        // stalling the game thread for the full bounded wait and then freeing the trampoline it is
        // still executing in.
        class Dispatching
        {
        public:
            explicit Dispatching(CubeHook hook);
            ~Dispatching();

            Dispatching(const Dispatching&) = delete;
            Dispatching& operator=(const Dispatching&) = delete;

        private:
            CubeHook m_hook;
        };

        bool insideDetour(CubeHook hook);
    }
}
