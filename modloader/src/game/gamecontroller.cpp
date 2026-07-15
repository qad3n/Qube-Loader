#include "game/gamecontroller.h"
#include "game/offsets.h"
#include "game/framecache.h"
#include "core/mem.h"
#include "core/log.h"
#include "util/fmt.h"

#include <cstdint>

namespace game
{
    namespace
    {

        constexpr char kCategory[] = "resolve";

        // Last-logged resolution, so the chain walk is logged only when it CHANGES (resolves, is
        // lost, or the base pointer moves) instead of every frame - full addresses without the flood.
        struct ResolveState
        {
            bool valid;
            bool ok;
            uintptr_t gc;
            uintptr_t creature;
        };

        ResolveState g_lastPlayer = {false, false, 0, 0};
        ResolveState g_lastGc = {false, false, 0, 0};

        bool resolveLocalPlayerLive(uintptr_t& gcOut, uintptr_t& creatureOut, const char*& reason)
        {
            reason = "";
            if (!off::kLocalPlayerPtr)
            {
                reason = "no kLocalPlayerPtr offset";
                return false;
            }

            const uintptr_t globalLive = mem::rebase(off::kLocalPlayerPtr);
            uint32_t gc = 0;
            if (!mem::read(globalLive, gc) || !gc)
            {
                reason = "global slot null/unreadable";
                return false;
            }

            uint32_t creature = gc;
            if (off::kLocalPlayerChain != off::kNoChain)
            {
                if (!mem::read(gc + off::kLocalPlayerChain, creature) || !creature)
                {
                    reason = "GC+chain null (title screen / not in world)";
                    return false;
                }
            }

            if (!mem::readable(reinterpret_cast<const void*>(creature), off::kPlayerStructMinSize))
            {
                reason = "creature base not readable";
                return false;
            }

            // Real Creature has its vftable at offset 0; rejects the uninitialized
            // title-screen slot (would read garbage otherwise).
            uint32_t vtable = 0;
            if (!mem::read(creature, vtable) || vtable != static_cast<uint32_t>(mem::rebase(off::kCreatureVtable)))
            {
                reason = "vtable mismatch (uninitialized slot)";
                return false;
            }

            gcOut = gc;
            creatureOut = creature;
            return true;
        }

        void logPlayerTransition(const framecache::PlayerResolve& r, const char* reason)
        {
            if (g_lastPlayer.valid && g_lastPlayer.ok == r.ok && g_lastPlayer.gc == r.gc && g_lastPlayer.creature == r.creature)
                return;

            g_lastPlayer = {true, r.ok, r.gc, r.creature};
            if (r.ok)
                LOGC(Debug, kCategory, "local player resolved: [0x%08X] -> GC 0x%08X -> +0x%X -> creature 0x%08X (vtable 0x%08X)",
                     fmt::u32(mem::rebase(off::kLocalPlayerPtr)), fmt::u32(r.gc),
                     static_cast<unsigned>(off::kLocalPlayerChain), fmt::u32(r.creature),
                     fmt::u32(mem::rebase(off::kCreatureVtable)));
            else
                LOGC(Debug, kCategory, "local player unavailable: %s (global [0x%08X])",
                     reason, fmt::u32(mem::rebase(off::kLocalPlayerPtr)));
        }

        bool resolveGameControllerLive(uintptr_t& gcOut, const char*& reason)
        {
            reason = "";
            if (!off::kWorldPtr)
            {
                reason = "no kWorldPtr offset";
                return false;
            }

            const uintptr_t globalLive = mem::rebase(off::kWorldPtr);
            uint32_t gc = 0;
            if (!mem::read(globalLive, gc) || !gc)
            {
                reason = "global slot null/unreadable";
                return false;
            }
            if (!mem::readable(reinterpret_cast<const void*>(gc), off::kWorldStructMinSize))
            {
                reason = "GC base not readable";
                return false;
            }

            gcOut = gc;
            return true;
        }

        void logGcTransition(bool ok, uintptr_t gc, const char* reason)
        {
            if (g_lastGc.valid && g_lastGc.ok == ok && g_lastGc.gc == gc)
                return;

            g_lastGc = {true, ok, gc, 0};
            if (ok)
                LOGC(Debug, kCategory, "GameController resolved: [0x%08X] -> GC 0x%08X (world embedded @ +0x%X)",
                     fmt::u32(mem::rebase(off::kWorldPtr)), fmt::u32(gc), static_cast<unsigned>(off::kWorldEmbedOff));
            else
                LOGC(Debug, kCategory, "GameController unavailable: %s (global [0x%08X])",
                     reason, fmt::u32(mem::rebase(off::kWorldPtr)));
        }

    }

    bool resolveLocalPlayer(uintptr_t& gcOut, uintptr_t& creatureOut)
    {
        framecache::PlayerResolve cached;
        if (framecache::getPlayer(cached))
        {
            gcOut = cached.gc;
            creatureOut = cached.creature;
            return cached.ok;
        }

        framecache::PlayerResolve resolved = {false, 0, 0};
        const char* reason = "";
        resolved.ok = resolveLocalPlayerLive(resolved.gc, resolved.creature, reason);
        framecache::putPlayer(resolved);
        logPlayerTransition(resolved, reason);

        gcOut = resolved.gc;
        creatureOut = resolved.creature;

        return resolved.ok;
    }

    bool resolveGameController(uintptr_t& gcOut)
    {
        bool ok = false;
        uintptr_t gc = 0;
        if (framecache::getGameController(ok, gc))
        {
            gcOut = gc;
            return ok;
        }

        const char* reason = "";
        ok = resolveGameControllerLive(gc, reason);
        framecache::putGameController(ok, gc);
        logGcTransition(ok, gc, reason);
        gcOut = gc;
        return ok;
    }

    // Uncached identity check: is `address` the local player's Creature? Used from game-thread hooks
    // (e.g. the crit detour) that must NOT touch the render-thread frame cache.
    bool isLocalPlayerCreature(uint32_t address)
    {
        if (!address)
            return false;

        uintptr_t gc = 0;
        uintptr_t creature = 0;
        const char* reason = "";
        return resolveLocalPlayerLive(gc, creature, reason) && creature == static_cast<uintptr_t>(address);
    }

}
