#include "game/framecache.h"

#include <cstring>

// g_active is thread_local, true on the render thread alone; other threads miss getX()
// and no-op putX(). So the cache state below is render-thread-only and needs no lock.
namespace game::framecache
{
    namespace
    {
        thread_local bool g_active = false;

        bool g_havePlayer = false;
        PlayerResolve g_player = {false, 0, 0};
        bool g_haveGc = false;
        bool g_gcOk = false;
        uintptr_t g_gc = 0;
        bool g_haveEntities = false;
        int32_t g_entityCount = 0;
        CubeEntity g_entities[CUBE_ENTITIES_MAX];

    }

    void beginFrame()
    {
        g_active = true;
        g_havePlayer = false;
        g_haveGc = false;
        g_haveEntities = false;
    }

    bool getPlayer(PlayerResolve& out)
    {
        if (!g_active || !g_havePlayer)
            return false;
        out = g_player;
        return true;
    }

    void putPlayer(const PlayerResolve& in)
    {
        if (!g_active)
            return;
        g_player = in;
        g_havePlayer = true;
    }

    bool getGameController(bool& okOut, uintptr_t& gcOut)
    {
        if (!g_active || !g_haveGc)
            return false;
        okOut = g_gcOk;
        gcOut = g_gc;
        return true;
    }

    void putGameController(bool ok, uintptr_t gc)
    {
        if (!g_active)
            return;

        g_gcOk = ok;
        g_gc = gc;
        g_haveGc = true;
    }

    int32_t getEntities(CubeEntity* out, int32_t maxCount)
    {
        if (!g_active || !g_haveEntities || !out || maxCount <= 0 || g_entityCount <= 0)
            return -1;
        const int32_t n = g_entityCount < maxCount ? g_entityCount : maxCount;
        std::memcpy(out, g_entities, static_cast<size_t>(n) * sizeof(CubeEntity));
        return n;
    }

    void putEntities(const CubeEntity* list, int32_t count)
    {
        if (!g_active || !list || count < 0)
            return;
        const int32_t n = count < CUBE_ENTITIES_MAX ? count : CUBE_ENTITIES_MAX;
        if (n > 0)
            std::memcpy(g_entities, list, static_cast<size_t>(n) * sizeof(CubeEntity));
        g_entityCount = n;
        g_haveEntities = true;
    }
}