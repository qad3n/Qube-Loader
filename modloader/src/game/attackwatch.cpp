#include "game/attackwatch.h"
#include "game/creature.h"
#include "game/catalog.h"
#include "game/offsets.h"
#include "core/mem.h"

#include <atomic>
#include <cstdint>

namespace game::attackwatch
{
    namespace
    {
        std::atomic<uint32_t> g_localPlayer{0}; // render thread writes, game thread reads
        std::atomic<uint32_t> g_seq{0}; // bumped on each attack edge (game thread)
        std::atomic<int32_t> g_lastAction{0}; // action id captured at the most recent edge
        std::atomic<bool> g_active{false};
        // Last sampled state of the player's action, so each individual swing is an edge.
        std::atomic<bool> g_prevPrimary{false};
        std::atomic<int32_t> g_prevAction{0};
        std::atomic<int32_t> g_prevElapsed{0};
        uint32_t g_seenSeq = 0; // render thread only

        // A PRIMARY attack (basic weapon swing/shot): a combat action that is NOT a special hotbar
        // ability (those are reported separately via the cooldown-map diff). The ability set is exactly
        // the CUBE_CATALOG_ABILITY entries, so a named ability id is excluded here.
        bool isPrimaryAttack(uintptr_t creature, uint8_t action)
        {
            if (!game::isCombatActionId(creature, action))
                return false;
            return catalogName(CUBE_CATALOG_ABILITY, static_cast<int32_t>(action)) == nullptr;
        }

        void resetEdgeState()
        {
            g_prevPrimary.store(false, std::memory_order_relaxed);
            g_prevAction.store(0, std::memory_order_relaxed);
            g_prevElapsed.store(0, std::memory_order_relaxed);
        }
    }

    void setActive(bool on)
    {
        g_active.store(on, std::memory_order_release);
    }

    bool active()
    {
        return g_active.load(std::memory_order_acquire);
    }

    void setLocalPlayer(uint32_t creature)
    {
        const uint32_t prev = g_localPlayer.exchange(creature, std::memory_order_relaxed);
        // Reset the edge state when the player object changes/leaves so no false edge crosses the gap.
        if (creature != prev)
            resetEdgeState();
    }

    void onBehaviorTick(uint32_t creature)
    {
        if (!creature || creature != g_localPlayer.load(std::memory_order_relaxed))
            return;

        uint8_t action = 0;
        if (!mem::read(static_cast<uintptr_t>(creature) + off::kPlayerActionOff, action))
            return;
        int32_t elapsed = 0;
        mem::read(static_cast<uintptr_t>(creature) + off::kPlayerActionElapsedOff, elapsed);

        const bool primary = isPrimaryAttack(static_cast<uintptr_t>(creature), action);
        const bool prevPrimary = g_prevPrimary.exchange(primary, std::memory_order_relaxed);
        const int32_t prevAction = g_prevAction.exchange(static_cast<int32_t>(action), std::memory_order_relaxed);
        const int32_t prevElapsed = g_prevElapsed.exchange(elapsed, std::memory_order_relaxed);

        if (!primary)
            return;

        // One edge per swing/shot: entered a primary attack (first swing / discrete shot), the swing
        // anim changed (combo step to a different anim), or the elapsed timer re-zeroed while still
        // attacking (a new swing of the same anim). The chained-combo case is exactly the last one:
        // +0x68 never returns to idle between swings, but +0x6c is reset at the start of each swing.
        const bool entered = !prevPrimary;
        const bool animChanged = prevPrimary && static_cast<int32_t>(action) != prevAction;
        const bool elapsedReset = prevPrimary && elapsed < prevElapsed;
        if (entered || animChanged || elapsedReset)
        {
            g_lastAction.store(static_cast<int32_t>(action), std::memory_order_relaxed);
            g_seq.fetch_add(1, std::memory_order_release);
        }
    }

    bool pollAttack(int32_t& actionId, uint32_t& count)
    {
        const uint32_t seq = g_seq.load(std::memory_order_acquire);
        if (seq == g_seenSeq)
            return false;

        count = seq - g_seenSeq;
        g_seenSeq = seq;
        actionId = g_lastAction.load(std::memory_order_relaxed);
        return true;
    }
}
