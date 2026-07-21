#pragma once
// Runs risky logic and logs any C++ exception instead of letting it escape. The owner overload also
// isolates CPU faults so a broken mod is disabled, not crash the game (see core/faultguard.h).
#include <exception>
#include <utility>
#include "core/log.h"
#include "core/faultguard.h"

namespace guard
{
    // Loader-internal guard: catches C++ exceptions only (mingw has no SEH). A fault here is a
    // loader bug and is left to crash.cpp, not silently swallowed.
    template <typename Fn>
    bool tryRun(const char* what, Fn&& fn)
    {
        try
        {
            fn();
            return true;
        }
        catch (const std::exception& e)
        {
            LOGC(Error, "guard", "%s failed: %s", what, e.what());
            return false;
        }
        catch (...)
        {
            LOGC(Error, "guard", "%s failed: unknown exception", what);
            return false;
        }
    }

    // Mod-callback guard: attributes work to owner and, when fault isolation is on, recovers from a
    // CPU fault by quarantining that mod. Falls back to the plain guard when owner==null or off.
    template <typename Fn>
    bool tryRun(const char* what, const CubeApi* owner, Fn&& fn)
    {
        if (!owner || !faultguard::enabled())
            return tryRun(what, std::forward<Fn>(fn));

        Fn* fnPtr = &fn;

        return faultguard::runGuarded(what, owner,[](void* ctx) { (*static_cast<Fn*>(ctx))(); }, fnPtr);
    }

    // Loader game-thread guard: isolates a CPU fault in the loader's OWN game-thread detour bodies
    // (no mod owner) so the game thread recovers instead of dying silently. Returns false on fault so
    // the caller can apply a safe fallback (e.g. the vanilla result). Falls back to the C++-only guard
    // when isolation is off. Distinct from the owner form: no quarantine/strike, just recover + log.
    template <typename Fn>
    bool tryRunLoader(const char* what, Fn&& fn)
    {
        if (!faultguard::enabled())
            return tryRun(what, std::forward<Fn>(fn));

        Fn* fnPtr = &fn;

        return faultguard::runGuarded(what, nullptr, [](void* ctx) { (*static_cast<Fn*>(ctx))(); }, fnPtr);
    }
}