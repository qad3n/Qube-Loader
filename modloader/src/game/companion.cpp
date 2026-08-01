#include "game/companion.h"
#include "game/creature.h"
#include "game/gamecontroller.h"
#include "game/creatures.h"
#include "game/offsets.h"
#include "util/field.h"
#include "core/mem.h"

#include <cstdint>

namespace game
{
    bool readCompanion(CubeCompanion& out)
    {
        uintptr_t gc = 0;
        uintptr_t player = 0;
        if (!resolveLocalPlayer(gc, player))
            return false;

        uint64_t companionId = 0;
        if (!mem::read(player + off::kCompanionIdOff, companionId) || !companionId)
            return false; // no active/summoned pet

        uintptr_t pet = 0;
        if (!findCreatureById(gc, companionId, pet))
            return false; // pet id set but the live creature is not present

        out.structSize = sizeof(CubeCompanion);
        out.address = static_cast<uint32_t>(pet);

        field::i32(pet, off::kPlayerTypeOff, out.type);
        field::i32(pet, off::kPlayerLevelOff, out.level);
        field::u32(pet, off::kPlayerXpOff, out.xp);
        field::f32(pet, off::kPlayerHealthOff, out.health);

        resolveDisplayName(pet, out.type, out.name);
        out.hasName = 1;
        readCreaturePosition(pet, out.x, out.y, out.z);

        out.hasPosition = 1;
        out.creatureState = (out.health > kDeadHealth) ? CUBE_CREATURESTATE_ALIVE : CUBE_CREATURESTATE_DEAD;

        readCreatureFacing(pet, out.facing);
        readCreatureVelocity(pet, out.velX, out.velY, out.velZ);

        return true;
    }
}