#pragma once
// Build-identity guard for the loader's pinned game-function detours. Every inline hook targets a hard
// address from one specific Cube.exe build (see CLAUDE.md "Target facts"); on any other build those
// addresses land mid-function and MinHook would corrupt unrelated code, crashing the game on launch.
// This compares the prologue bytes at each detour target against the reference build and refuses to
// hook a mismatch. Windows-first: the reference bytes come straight from the shipped binary, so the
// check is identical on Windows and Wine.
#include <cstdint>

namespace game::signature
{
    // True iff the loaded binary matches the reference prologue at `staticAddr` (a known pinned detour
    // target). Unknown addresses (e.g. a raw mod-supplied hook) return true - they are not our sites.
    // Guarded read; false when the bytes differ or the address is unreadable.
    bool verifyTarget(uintptr_t staticAddr);

    // One-shot, cached whole-binary check across every pinned detour site; logs a report the first time.
    // MUST be called before any detour patches a site (MinHook overwrites the prologue). The loader
    // calls it early (gamelog install + installModHooks), so later calls just return the cached result.
    bool compatibleBuild();
}
