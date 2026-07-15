#pragma once
// Detours GameController::updateSelectedEntity (the R / use key) to record the last entity the
// player selected. The detour captures state on the game thread; the render-thread poll emits it.

#include "cube_sdk.h"

namespace game::selection
{
    // Installs the select detour. Idempotent; false (and logs) if MinHook fails (non-fatal).
    bool install();
    void remove();

    // Fills the most recent selection record; false if nothing has been selected yet.
    bool readLast(CubeSelection& out);

    // Render-thread poll: fills out + returns true if a new selection happened since last call.
    bool pollSelection(CubeSelection& out);
}
