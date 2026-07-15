#include "game/pet.h"
#include "game/creature.h"
#include "game/gamecontroller.h"
#include "game/entities.h"
#include "game/offsets.h"
#include "util/field.h"
#include "core/mem.h"

#include <cstdint>

namespace game
{
    bool readPet(CubePet& out)
    {
        uintptr_t gc = 0;
        uintptr_t player = 0;
        if (!resolveLocalPlayer(gc, player))
            return false;

        uint64_t petId = 0;
        if (!mem::read(player + off::kPetIdOff, petId) || !petId)
            return false; // no active/summoned pet

        uintptr_t pet = 0;
        if (!findCreatureById(gc, petId, pet))
            return false; // pet id set but the live creature is not present

        out.structSize = sizeof(CubePet);
        out.address = static_cast<uint32_t>(pet);

        field::i32(pet, off::kPlayerTypeOff, out.type);
        field::i32(pet, off::kPlayerLevelOff, out.level);
        field::u32(pet, off::kPlayerXpOff, out.xp);
        field::f32(pet, off::kPlayerHealthOff, out.health);

        out.hasName = resolveDisplayName(pet, out.type, out.name);
        readCreaturePosition(pet, out.x, out.y, out.z);

        out.hasPosition = 1;
        out.entityState = (out.health > kDeadHealth) ? CUBE_ENTSTATE_ALIVE : CUBE_ENTSTATE_DEAD;

        readCreatureFacing(pet, out.facing);
        readCreatureVelocity(pet, out.velX, out.velY, out.velZ);

        return true;
    }
}