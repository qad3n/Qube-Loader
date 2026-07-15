#pragma once
// Local-player combat snapshot + per-frame damage/hit telemetry.

#include "cube_sdk.h"

namespace game
{
    bool readCombat(CubeCombat& out);

    // Called by the CRIT_ROLL detour (game thread) on each crit; folded into the crit
    // counter + PLAYER_CRIT edge by the next pollCombat (render thread).
    void noteCrit();

    // Per-creature edge detected this frame, so gameevents can emit the matching event.
    enum class EntityEdgeKind
    {
        Spawn, // address newly present in the entity map (created / entered range)
        Death, // a tracked creature's health crossed to <= 0 while still present (killed)
        Despawn, // a tracked address is gone from the map (destroyed / left range)
        Stunned, // a tracked creature's stun-lock timer became active
        Recovered, // a tracked creature's stun-lock timer ended (can act again)
        KnockedDown, // a tracked creature entered the downed state
        Damaged // a tracked creature lost health this frame (amount = how much)
    };

    struct EntityEdge
    {
        uint32_t address;
        EntityEdgeKind kind;
        float damage = 0.0f; // Damaged: health lost this frame; 0 for the other kinds
        float health = 0.0f; // the creature's remaining health at this edge (last-known for Despawn)
        int32_t category = 0; // the creature's kind byte (+0x60); last-known for Despawn
        int32_t type = 0; // the creature's type/species/model id (+0x64); last-known for Despawn
    };

    // Edges detected this frame, so the caller (gameevents) can emit events.
    struct CombatEdges
    {
        float damageTaken; // local player's health lost this frame (0 = none)
        bool crit; // a crit roll landed since last frame
    };

    // Diffs the player's + nearby creatures' health vs last frame: updates the counters and
    // fills up to maxEdges into edgesOut (edgeCount = number written). Resets when the player
    // is unavailable, which also suppresses a spawn flood the first frame back in world.
    CombatEdges pollCombat(const CubePlayer& player, bool playerValid, EntityEdge* edgesOut, int32_t maxEdges, int32_t& edgeCount);
}
