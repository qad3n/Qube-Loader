#include "game/combat.h"
#include "game/creature.h"
#include "game/gamecontroller.h"
#include "game/entities.h"
#include "game/offsets.h"
#include "util/field.h"

#include <atomic>
#include <cstdint>

namespace game
{
    namespace
    {
        // One nearby creature's state, remembered across frames to detect edges.
        struct TrackedEntity
        {
            uint32_t address;
            float health;
            int32_t category;
            int32_t type;
            bool stunned;
            bool knockedDown;
        };

        // pollCombat writes on the render thread; readCombat may read from a game-thread hook,
        // so the copied-out scalars are atomic. tracked[]/trackedCount are render-thread-only.
        struct CombatStore
        {
            bool valid = false;
            std::atomic<float> health{0.0f};
            std::atomic<float> lastDamageTaken{0.0f};
            std::atomic<float> lastDamageDealt{0.0f};
            std::atomic<uint32_t> hits{0};
            std::atomic<uint32_t> crits{0};
            std::atomic<uint32_t> damageTakenEvents{0};
            TrackedEntity tracked[CUBE_ENTITIES_MAX];
            int32_t trackedCount = 0;
        };

        CombatStore g_store;

        // Crit rolls from the CRIT_ROLL detour (game thread), folded into g_store.crits by
        // pollCombat (render thread). Atomic exchange bridges the two threads.
        std::atomic<uint32_t> g_critPending{0};

        int32_t previousIndexOf(uint32_t address)
        {
            for (int32_t i = 0; i < g_store.trackedCount; ++i)
            {
                if (g_store.tracked[i].address == address)
                    return i;
            }
            return -1;
        }

        bool addressInList(uint32_t address, const CubeEntity* entities, int32_t count)
        {
            for (int32_t i = 0; i < count; ++i)
            {
                if (entities[i].address == address)
                    return true;
            }
            return false;
        }

    }

    void noteCrit()
    {
        g_critPending.fetch_add(1, std::memory_order_relaxed);
    }

    CombatEdges pollCombat(const CubePlayer& player, bool playerValid, EntityEdge* edgesOut, int32_t maxEdges, int32_t& edgeCount)
    {
        CombatEdges edges = {0.0f, false};
        edgeCount = 0;
        // Fold crit rolls recorded since last frame (always, so the counter never drifts);
        // a nonzero delta is this frame's PLAYER_CRIT edge.
        const uint32_t newCrits = g_critPending.exchange(0, std::memory_order_relaxed);
        if (newCrits)
        {
            g_store.crits.fetch_add(newCrits, std::memory_order_relaxed);
            edges.crit = true;
        }
        if (!playerValid)
        {
            g_store.valid = false;
            g_store.trackedCount = 0;
            return edges;
        }

        const bool hadPrev = g_store.valid;
        const float prevHealth = g_store.health.load(std::memory_order_relaxed);

        if (hadPrev && player.health < prevHealth)
        {
            const float taken = prevHealth - player.health;
            g_store.lastDamageTaken.store(taken, std::memory_order_relaxed);
            g_store.damageTakenEvents.fetch_add(1, std::memory_order_relaxed);
            edges.damageTaken = taken;
        }

        // One entity-map walk feeds hit detection + spawn/death edges (a hit = any nearby
        // creature losing health). Static, not a ~125KB stack array (overflow = uncatchable, no SEH).
        static CubeEntity entities[CUBE_ENTITIES_MAX];
        const int32_t count = listEntities(entities, CUBE_ENTITIES_MAX);

        if (hadPrev)
        {
            for (int32_t i = 0; i < count; ++i)
            {
                const uint32_t address = entities[i].address;
                const float health = entities[i].health;
                const int32_t category = entities[i].category;
                const int32_t type = entities[i].type;
                const bool stunned = entities[i].hitStun > 0;
                const bool knockedDown = entities[i].knockedDown != 0;

                const int32_t prev = previousIndexOf(address);
                if (prev < 0)
                {
                    if (edgesOut && edgeCount < maxEdges)
                        edgesOut[edgeCount++] = EntityEdge{address, EntityEdgeKind::Spawn, 0.0f, health, category, type};
                    continue; // brand-new creature has no previous state to diff
                }

                const TrackedEntity& was = g_store.tracked[prev];
                if (health < was.health)
                {
                    const float dealt = was.health - health;
                    g_store.lastDamageDealt.store(dealt, std::memory_order_relaxed);
                    g_store.hits.fetch_add(1, std::memory_order_relaxed);
                    if (edgesOut && edgeCount < maxEdges)
                        edgesOut[edgeCount++] = EntityEdge{address, EntityEdgeKind::Damaged, dealt, health, category, type};
                }

                if (was.health > kDeadHealth && health <= kDeadHealth && edgesOut && edgeCount < maxEdges)
                    edgesOut[edgeCount++] = EntityEdge{address, EntityEdgeKind::Death, 0.0f, health, category, type};
                if (!was.stunned && stunned && edgesOut && edgeCount < maxEdges)
                    edgesOut[edgeCount++] = EntityEdge{address, EntityEdgeKind::Stunned, 0.0f, health, category, type};
                if (was.stunned && !stunned && edgesOut && edgeCount < maxEdges)
                    edgesOut[edgeCount++] = EntityEdge{address, EntityEdgeKind::Recovered, 0.0f, health, category, type};
                if (!was.knockedDown && knockedDown && edgesOut && edgeCount < maxEdges)
                    edgesOut[edgeCount++] = EntityEdge{address, EntityEdgeKind::KnockedDown, 0.0f, health, category, type};
            }
            // Despawn: tracked last frame but gone from the map now. Uses the OLD tracked set.
            for (int32_t i = 0; i < g_store.trackedCount; ++i)
            {
                const TrackedEntity& was = g_store.tracked[i];
                if (!addressInList(was.address, entities, count) && edgesOut && edgeCount < maxEdges)
                    edgesOut[edgeCount++] = EntityEdge{was.address, EntityEdgeKind::Despawn, 0.0f, was.health, was.category, was.type};
            }
        }

        g_store.trackedCount = count < CUBE_ENTITIES_MAX ? count : CUBE_ENTITIES_MAX;
        for (int32_t i = 0; i < g_store.trackedCount; ++i)
        {
            g_store.tracked[i].address = entities[i].address;
            g_store.tracked[i].health = entities[i].health;
            g_store.tracked[i].category = entities[i].category;
            g_store.tracked[i].type = entities[i].type;
            g_store.tracked[i].stunned = entities[i].hitStun > 0;
            g_store.tracked[i].knockedDown = entities[i].knockedDown != 0;
        }

        g_store.health.store(player.health, std::memory_order_relaxed);
        g_store.valid = true;
        return edges;
    }

    bool readCombat(CubeCombat& out)
    {
        uintptr_t gc = 0;
        uintptr_t obj = 0;
        if (!resolveLocalPlayer(gc, obj))
            return false;

        out.address = static_cast<uint32_t>(obj);
        field::f32(obj, off::kPlayerBaseDamageOff, out.baseDamage);
        field::f32(obj, off::kPlayerPowerOff, out.power);
        field::f32(obj, off::kPlayerArmorOff, out.armor);
        field::f32(obj, off::kPlayerSpiritOff, out.spirit);
        field::i32(obj, off::kPlayerComboOff, out.combo);
        field::f32(obj, off::kPlayerAttackCooldownOff, out.attackCooldown);
        field::f32(obj, off::kPlayerAttackSpeedOff, out.attackSpeed);
        field::f32(obj, off::kPlayerStealthOff, out.critStat); // +0x1190 stealth stat also feeds crit
        out.critChancePercent = out.critStat * off::kCritChancePerPoint * off::kPercentScale;
        field::i32(obj, off::kPlayerHitStunOff, out.hitStun);
        out.lastDamageTaken = g_store.lastDamageTaken.load(std::memory_order_relaxed);
        out.lastDamageDealt = g_store.lastDamageDealt.load(std::memory_order_relaxed);
        out.hits = g_store.hits.load(std::memory_order_relaxed);
        out.crits = g_store.crits.load(std::memory_order_relaxed);
        out.damageTakenEvents = g_store.damageTakenEvents.load(std::memory_order_relaxed);
        out.hasCombat = 1;

        return true;
    }

}