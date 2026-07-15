#pragma once
// Detours GameController::onItemPickup (the E / hold-to-pickup action) to record the last item the
// player picked up. The detour reads the staged item POD on the game thread; the render-thread poll
// emits the event.

#include "cube_sdk.h"

namespace game::pickup
{
    // Installs the pickup detour. Idempotent; false (and logs) if MinHook fails (non-fatal).
    bool install();
    void remove();

    // Fills the most recently picked-up item; false if nothing has been picked up yet.
    bool readLast(CubeItem& out);

    // Render-thread poll: fills out + returns true if a new pickup happened since last call.
    bool pollPickup(CubeItem& out);
}
