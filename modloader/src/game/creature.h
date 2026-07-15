#pragma once
// Generic per-Creature reads/writes shared by the player/pet/entity/status readers. Every creature
// shares the Creature memory layout, so these operate on any validated Creature address.

#include "cube_sdk.h"
#include <cstdint>

namespace game
{
    // Shared across the creature readers (player/pet/entity).
    constexpr uint32_t kMaxNameChars = CUBE_PLAYER_NAME_MAX - 1;
    constexpr float kDeadHealth = 0.0f;

    // Reads the Creature position triple (int64 fixed-point) into floats.
    void readCreaturePosition(uintptr_t creature, float& x, float& y, float& z);
    // Reads the Creature velocity triple (three contiguous floats).
    void readCreatureVelocity(uintptr_t creature, float& x, float& y, float& z);
    // Reads + wraps the creature body yaw (raw field grows unbounded).
    void readCreatureFacing(uintptr_t creature, float& facing);

    // Creature-level writes, shared by the player + entity/pet setters.
    bool writeCreatureStat(uintptr_t obj, int32_t stat, double value);
    bool setCreaturePosition(uintptr_t obj, float x, float y, float z);
    bool setCreatureName(uintptr_t obj, const char* name);
    // Validates address is a live Creature (readable + vftable match); guards entity/pet writes
    // against arbitrary addresses.
    bool validateCreature(uint32_t address, uintptr_t& objOut);
    // Resolves a target to a Creature: 0 = local player, else validate. False if unavailable.
    bool resolveCreatureOrLocal(uint32_t address, uintptr_t& objOut);
    // Two-tier display name: inline name (players/NPCs) else species-catalog name else "species #n".
    int32_t resolveDisplayName(uintptr_t creature, int32_t typeId, char* out);
    // Is `action` an in-progress combat action (attack/shot/cast/ability) for this creature? Uses the
    // game's own windup classifier (no attack-id whitelist). Safe to call from a game-thread hook.
    bool isCombatActionId(uintptr_t creature, uint8_t action);
}
