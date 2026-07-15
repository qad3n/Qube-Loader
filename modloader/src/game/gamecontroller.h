#pragma once
// Resolves the pinned [0x76b1c8]->GameController->local Creature chain (validated by the Creature
// vftable), the single owner of live-player resolution. Render-thread reads are frame-cached.

#include <cstdint>

namespace game
{
    // Resolves GameController + local Creature (validated by vftable). Both out on true.
    bool resolveLocalPlayer(uintptr_t& gcOut, uintptr_t& creatureOut);
    // Resolves just the GameController (no in-world player required).
    bool resolveGameController(uintptr_t& gcOut);
    // Uncached identity check (safe from game-thread hooks): is address the local player's Creature?
    bool isLocalPlayerCreature(uint32_t address);
}
