#include "game/entities.h"
#include "game/creature.h"
#include "game/gamecontroller.h"
#include "game/items.h"
#include "game/catalog.h"
#include "game/offsets.h"
#include "util/field.h"
#include "game/framecache.h"
#include "core/mem.h"
#include "util/math.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace game
{
    namespace
    {
        mathutil::Vec3 readPosition(uintptr_t creature)
        {
            mathutil::Vec3 out = {};
            readCreaturePosition(creature, out.x, out.y, out.z);
            return out;
        }

        // MSVC _Tree in-order successor. Leaf child pointers point at _Myhead (head),
        // which serves as the nil sentinel; head is returned to signal "end".
        uint32_t successor(uint32_t node, uint32_t head)
        {
            uint32_t right = 0;
            if (!mem::read(node + off::kRbRight, right))
                return head;

            uint32_t steps = 0;
            if (right != head)
            {
                node = right;
                uint32_t left = 0;
                while (mem::read(node + off::kRbLeft, left) && left != head)
                {
                    node = left;
                    if (++steps > off::kMaxEntityWalk)
                        return head; // corrupt/cyclic tree; bail rather than spin
                }
                return node;
            }

            uint32_t parent = 0;
            if (!mem::read(node + off::kRbParent, parent))
                return head;

            uint32_t parentRight = 0;
            while (parent != head && mem::read(parent + off::kRbRight, parentRight) && node == parentRight)
            {
                node = parent;
                if (!mem::read(node + off::kRbParent, parent) || ++steps > off::kMaxEntityWalk)
                    return head;
            }
            return parent;
        }

        template <typename Fn>
        void forEachEntity(uintptr_t gc, Fn fn)
        {
            uint32_t head = 0;
            if (!mem::read(gc + off::kEntityMapHead, head) || !head)
                return;

            uint32_t node = 0;
            if (!mem::read(head + off::kRbLeft, node)) // begin() = _Myhead->_Left (leftmost)
                return;

            uint32_t steps = 0;
            while (node != head && steps < off::kMaxEntityWalk)
            {
                ++steps;
                uint32_t creature = 0;
                if (mem::read(node + off::kRbValue, creature) && creature)
                {
                    uint64_t key = 0;
                    mem::read(node + off::kRbKey, key);
                    fn(creature, key);
                }
                node = successor(node, head);
            }
        }

        bool isBossSpecies(int32_t type)
        {
            for (int32_t id : off::kBossSpecies)
            {
                if (id == type)
                    return true;
            }
            return false;
        }

        // Passive-critter faction test: a kind==1 creature in the peaceful species set
        // (and whose state word lacks the disqualifying bits) is wildlife, not a monster.
        bool isPassiveCritter(uintptr_t creature, int32_t species)
        {
            uint16_t stateWord = 0;
            if (mem::read(creature + off::kStateWordOff, stateWord) && (stateWord & off::kPassiveFactionStateMask))
                return false;

            for (int32_t id : off::kPassiveSpecies)
            {
                if (id == species)
                    return true;
            }
            return false;
        }

        // Kind 1 splits by the passive-critter test (monster -> HOSTILE, animal -> NEUTRAL).
        // Kind 3 is unlabeled in the binary, so it stays UNKNOWN (never guessed).
        CubeRelation resolveRelation(uintptr_t creature, int32_t kind, int32_t species)
        {
            switch (kind)
            {
                case off::kKindPlayer:
                    return CUBE_REL_PLAYER;
                case off::kKindMonster:
                    return isPassiveCritter(creature, species) ? CUBE_REL_NEUTRAL : CUBE_REL_HOSTILE;
                case off::kKindPet:
                    return CUBE_REL_OWN_PET;
                case off::kKindNpc:
                    return CUBE_REL_NPC;
                default:
                    return CUBE_REL_UNKNOWN;
            }
        }

        void fillEntity(uintptr_t creature, uint64_t, const mathutil::Vec3& playerPos, uint64_t, CubeEntity& out)
        {
            out = CubeEntity{}; // caller buffer may be uninitialized; skipped reads must read 0
            out.structSize = sizeof(CubeEntity);
            out.address = static_cast<uint32_t>(creature);

            field::byteI32(creature, off::kEntityKindOff, out.category);
            field::i32(creature, off::kPlayerTypeOff, out.type);
            field::i32(creature, off::kPlayerLevelOff, out.level);
            field::f32(creature, off::kPlayerHealthOff, out.health);

            out.hasName = resolveDisplayName(creature, out.type, out.name);

            const mathutil::Vec3 pos = readPosition(creature);
            out.x = pos.x;
            out.y = pos.y;
            out.z = pos.z;
            out.distance = mathutil::distance3(playerPos, pos);

            // hostile flag = HOSTILE relation only (aggressive monster); passive animals are NEUTRAL.
            out.relation = resolveRelation(creature, out.category, out.type);
            out.hostile = (out.relation == CUBE_REL_HOSTILE) ? 1 : 0;
            out.entityState = (out.health > kDeadHealth) ? CUBE_ENTSTATE_ALIVE : CUBE_ENTSTATE_DEAD;
            out.boss = isBossSpecies(out.type) ? 1 : 0;
            readCreatureFacing(creature, out.facing);
            readCreatureVelocity(creature, out.velX, out.velY, out.velZ);
            readEquipmentSlot(creature, off::kEquipWeaponSlot, out.weapon);

            // combatClass is the archetype byte (+0x140); monster power tier is the star rank.
            // elite = boss or a high star rank.
            field::byteI32(creature, off::kPlayerClassOff, out.combatClass);
            field::byteI32(creature, off::kEntityRankOff, out.rank);

            out.elite = (out.boss || out.rank >= off::kEliteStarRank) ? 1 : 0;
            out.effectivePower = out.level / off::kEffectivePowerLevelDiv + out.rank + off::kEffectivePowerBase;

            field::i32(creature, off::kPlayerHitStunOff, out.hitStun);
            uint8_t action = 0;

            if (mem::read(creature + off::kPlayerActionOff, action))
                out.knockedDown = (action == off::kActionKnockdown) ? 1 : 0;
            // ownerAddress left 0: no confirmed pet->owner back-pointer. Own-pet is
            // identified by matching the player's pet id against the map key.
        }

        uint64_t readPetId(uintptr_t player)
        {
            uint64_t petId = 0;
            mem::read(player + off::kPetIdOff, petId);
            return petId;
        }

        bool closerToPlayer(const CubeEntity& a, const CubeEntity& b)
        {
            return a.distance < b.distance;
        }

    }

    int32_t listEntities(CubeEntity* out, int32_t maxCount)
    {
        if (!out || maxCount <= 0)
            return 0;

        // Served from the intra-frame cache on a hit (nearest-first prefix); miss -> live walk.
        const int32_t cached = framecache::getEntities(out, maxCount);
        if (cached >= 0)
            return cached;

        uintptr_t gc = 0;
        uintptr_t player = 0;
        if (!resolveLocalPlayer(gc, player))
            return 0;

        const mathutil::Vec3 playerPos = readPosition(player);
        const uint64_t petId = readPetId(player);
        int32_t count = 0;
        forEachEntity(gc, [&](uint32_t creature, uint64_t key)
        {
            if (count >= maxCount || creature == static_cast<uint32_t>(player))
                return;
            fillEntity(creature, key, playerPos, petId, out[count]);
            ++count;
        });

        // Nearest-first (index 0 = closest). The map is walked in id order, so a world with
        // >maxCount creatures keeps the first maxCount by id then sorts (not a true nearest-N cap).
        std::sort(out, out + count, closerToPlayer);
        // Cache only a full-buffer request, so a later larger request isn't served a truncated set.
        if (maxCount >= CUBE_ENTITIES_MAX)
            framecache::putEntities(out, count);
        return count;
    }

    // Mirrors the game's tameable predicate (FUN_00444680): the creature must be a passive-critter
    // species AND its state word must lack the disqualifying faction bits. Taming feeds it a Food
    // item (kItemTypeFood) via the R-interact.
    bool isCreatureTameable(uint32_t address)
    {
        uintptr_t obj = 0;
        if (!validateCreature(address, obj))
            return false;

        int32_t species = 0;
        if (!mem::read(obj + off::kPlayerTypeOff, species))
            return false;

        return isPassiveCritter(obj, species);
    }

    bool targetEntity(CubeEntity& out)
    {
        uintptr_t gc = 0;
        uintptr_t player = 0;
        if (!resolveLocalPlayer(gc, player))
            return false;

        uint32_t selected = 0;
        if (!mem::read(gc + off::kSelectedEntityOff, selected) || !selected)
            return false;
        uintptr_t obj = 0;
        if (!validateCreature(selected, obj))
            return false;

        const mathutil::Vec3 playerPos = readPosition(player);
        const uint64_t petId = readPetId(player);
        fillEntity(selected, 0, playerPos, petId, out);
        return true;
    }

    bool aimTargetEntity(CubeEntity& out)
    {
        uintptr_t gc = 0;
        uintptr_t player = 0;
        if (!resolveLocalPlayer(gc, player))
            return false;

        uint64_t aimId = 0;
        if (!mem::read(gc + off::kAimTargetIdOff, aimId) || !aimId)
            return false;
        uintptr_t creature = 0;
        if (!findCreatureById(gc, aimId, creature))
            return false;

        const mathutil::Vec3 playerPos = readPosition(player);
        const uint64_t petId = readPetId(player);
        fillEntity(creature, aimId, playerPos, petId, out);
        return true;
    }

    int32_t countPlayers(uintptr_t gc)
    {
        int32_t count = 0;
        forEachEntity(gc, [&](uint32_t creature, uint64_t)
        {
            int32_t kind = 0;
            field::byteI32(creature, off::kEntityKindOff, kind);
            if (kind == off::kKindPlayer)
                ++count;
        });
        return count;
    }

    bool findCreatureById(uintptr_t gc, uint64_t id, uintptr_t& creatureOut)
    {
        uint32_t found = 0;
        forEachEntity(gc, [&](uint32_t creature, uint64_t key)
        {
            if (key == id)
                found = creature;
        });
        if (!found)
            return false;
        creatureOut = found;
        return true;
    }

    bool setEntityStat(uint32_t address, int32_t stat, double value)
    {
        uintptr_t obj = 0;
        if (!validateCreature(address, obj))
            return false;
        return writeCreatureStat(obj, stat, value);
    }

    bool setEntityName(uint32_t address, const char* name)
    {
        uintptr_t obj = 0;
        if (!validateCreature(address, obj))
            return false;
        return setCreatureName(obj, name);
    }

    bool teleportEntity(uint32_t address, float x, float y, float z)
    {
        uintptr_t obj = 0;
        if (!validateCreature(address, obj))
            return false;
        return setCreaturePosition(obj, x, y, z);
    }
}