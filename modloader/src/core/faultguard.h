#pragma once
// CPU-fault isolation for mod callbacks (mingw has no SEH): a vectored handler longjmps out of a
// faulting callback, quarantines that mod, keeps the game running. Faults outside fall to crash.cpp.
#include "cube_sdk.h"

namespace faultguard
{
    typedef void (*GuardThunk)(void* ctx);

    // Register / unregister the vectored handler. Call install after crash::install so it sits
    // ahead of the crash filter; enable=false keeps C++-exception-only behavior.
    void install(bool enable);
    void remove();
    bool enabled();

    // Run fn(ctx) with fault isolation attributed to owner: catches C++ exceptions and recovers from
    // a CPU fault by quarantining owner (returns false). Out-of-line so the setjmp frame lives in one
    // place. Callers use guard::tryRun(what, owner, fn).
    bool runGuarded(const char* what, const CubeApi* owner, GuardThunk fn, void* ctx);

    // A quarantined mod's callbacks are skipped everywhere until teardown. Queried on the hot
    // dispatch path (fast no-op when nothing is quarantined).
    bool isQuarantined(const CubeApi* owner);

}