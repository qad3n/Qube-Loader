#pragma once
// Intra-frame resolve cache: beginFrame() opens a per-frame window caching the first player/GC/entity
// resolve. The window flag is thread-local (render thread), so a game-thread hook always resolves live.
#include "cube_sdk.h"
#include <cstdint>

namespace game::framecache
{
    struct PlayerResolve
    {
        bool ok;
        uintptr_t gc;
        uintptr_t creature;
    };

    // Render thread: open a fresh cache window (invalidates the prior one).
    void beginFrame();

    // Local-player / GameController resolve memo. get* returns false on a miss; the
    // caller resolves live and stores the result with put*.
    bool getPlayer(PlayerResolve& out);
    void putPlayer(const PlayerResolve& in);
    bool getGameController(bool& okOut, uintptr_t& gcOut);
    void putGameController(bool ok, uintptr_t gc);

    // Nearby-entity list memo. getEntities copies up to maxCount cached rows and
    // returns the count on a hit, or -1 on a miss. putEntities stores the full list.
    int32_t getEntities(CubeEntity* out, int32_t maxCount);
    void putEntities(const CubeEntity* list, int32_t count);
}