#include "game/creature.h"
#include "game/gamecontroller.h"
#include "game/offsets.h"
#include "util/field.h"
#include "game/catalog.h"
#include "core/mem.h"
#include "util/guard.h"
#include "util/math.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace game
{
    namespace
    {
        // Calls the game's own attack-windup classifier (getAttackWindup): a nonzero windup means
        // the creature is mid attack/shot/cast/ability, 0 means idle/locomotion. A pure leaf read on
        // the Creature, so it is safe to call for any validated creature. int __thiscall(this,int);
        // action < 0 selects the current action byte. On mingw a __thiscall is called as __fastcall
        // with an unused EDX.
        bool actionHasWindup(uintptr_t creature)
        {
            typedef int32_t(__fastcall* AttackWindupFn)(void* self, void* edx, int32_t action);
            const AttackWindupFn fn = reinterpret_cast<AttackWindupFn>(mem::rebase(off::kGetAttackWindupFn));
            int32_t windup = 0;
            guard::tryRun("attack windup", [&]() { windup = fn(reinterpret_cast<void*>(creature), nullptr, off::kUseCurrentAction); });
            return windup > 0;
        }

        // In-progress attack/cast/ability. Named non-attack states are excluded first; every other
        // id is decided by the game's windup classifier, so we never guess an attack-id whitelist.
        bool isCombatAction(uintptr_t creature, uint8_t action)
        {
            switch (action)
            {
                case off::kActionIdle:
                case off::kActionEat:
                case off::kActionBlock:
                case off::kActionKnockdown:
                    return false;
                default:
                    return actionHasWindup(creature);
            }
        }

    }

    void readCreaturePosition(uintptr_t creature, float& x, float& y, float& z)
    {
        field::vec3Fixed64(creature, off::kPlayerPosXOff, off::kPlayerPosScale, x, y, z);
    }

    void readCreatureVelocity(uintptr_t creature, float& x, float& y, float& z)
    {
        field::f32(creature, off::kPlayerVelXOff, x);
        field::f32(creature, off::kPlayerVelXOff + sizeof(float), y);
        field::f32(creature, off::kPlayerVelXOff + sizeof(float) * 2, z);
    }

    void readCreatureFacing(uintptr_t creature, float& facing)
    {
        field::f32(creature, off::kCreatureBodyYawOff, facing);
        facing = mathutil::wrapRadians(facing);
    }

    bool isCombatActionId(uintptr_t creature, uint8_t action)
    {
        return isCombatAction(creature, action);
    }

    bool setCreaturePosition(uintptr_t obj, float x, float y, float z)
    {
        if (!off::kPlayerPosXOff)
            return false;

        bool ok = field::setFixed64(obj, off::kPlayerPosXOff, off::kPlayerPosScale, x);
        ok = field::setFixed64(obj, off::kPlayerPosYOff, off::kPlayerPosScale, y) && ok;
        ok = field::setFixed64(obj, off::kPlayerPosZOff, off::kPlayerPosScale, z) && ok;

        return ok;
    }

    bool setCreatureName(uintptr_t obj, const char* name)
    {
        if (!name || !off::kPlayerNameOff)
            return false;

        char buffer[off::kPlayerNameCapacity]; // clamp to the inline name field; overflow corrupts the buff list
        std::snprintf(buffer, sizeof(buffer), "%s", name);
        return mem::writeRaw(obj + off::kPlayerNameOff, buffer, std::strlen(buffer) + 1);
    }

    // Writes a creature-local scalar stat (shared by player + entities). FACING here is body yaw;
    // the player's camera-yaw facing is special-cased in setPlayerStat.
    bool writeCreatureStat(uintptr_t obj, int32_t stat, double value)
    {
        const float f = static_cast<float>(value);
        const int32_t i = static_cast<int32_t>(value);
        const uint8_t b = static_cast<uint8_t>(i);

        switch (stat)
        {
            case CUBE_STAT_HEALTH:
                return field::setF32(obj, off::kPlayerHealthOff, f);
            case CUBE_STAT_MANA:
                return field::setF32(obj, off::kPlayerManaOff, f);
            case CUBE_STAT_STAMINA:
                return field::setF32(obj, off::kPlayerStaminaOff, f);
            case CUBE_STAT_COINS:
                return field::setI32(obj, off::kPlayerCoinsOff, i);
            case CUBE_STAT_LEVEL:
                return field::setI32(obj, off::kPlayerLevelOff, i);
            case CUBE_STAT_XP:
                return field::setI32(obj, off::kPlayerXpOff, i);
            case CUBE_STAT_CLASS:
                return field::setU8(obj, off::kPlayerClassOff, b);
            case CUBE_STAT_SPEC:
                return field::setU8(obj, off::kPlayerSpecOff, b);
            case CUBE_STAT_TYPE:
                return field::setI32(obj, off::kPlayerTypeOff, i);
            case CUBE_STAT_FACING:
                return field::setF32(obj, off::kCreatureBodyYawOff, f);
            case CUBE_STAT_POWER:
                return field::setF32(obj, off::kPlayerPowerOff, f);
            case CUBE_STAT_ARMOR:
                return field::setF32(obj, off::kPlayerArmorOff, f);
            case CUBE_STAT_SPIRIT:
                return field::setF32(obj, off::kPlayerSpiritOff, f);
            case CUBE_STAT_COMBO:
                return field::setI32(obj, off::kPlayerComboOff, i);
            case CUBE_STAT_ATTACK_COOLDOWN:
                return field::setF32(obj, off::kPlayerAttackCooldownOff, f);
            case CUBE_STAT_HITSTUN:
                return field::setI32(obj, off::kPlayerHitStunOff, i);
            case CUBE_STAT_VEL_X:
                return field::setF32(obj, off::kPlayerVelXOff, f);
            case CUBE_STAT_VEL_Y:
                return field::setF32(obj, off::kPlayerVelXOff + sizeof(float), f);
            case CUBE_STAT_VEL_Z:
                return field::setF32(obj, off::kPlayerVelXOff + sizeof(float) * 2, f);
            case CUBE_STAT_ACTION_ID:
                return field::setU8(obj, off::kPlayerActionOff, b);
            case CUBE_STAT_CATEGORY:
                return field::setU8(obj, off::kEntityKindOff, b);
            case CUBE_STAT_RANK:
                return field::setU8(obj, off::kEntityRankOff, b);
            case CUBE_STAT_ATTACK_SPEED:
                return field::setF32(obj, off::kPlayerAttackSpeedOff, f);
            case CUBE_STAT_CRIT:
                // Same field as stealth (+0x1190); it feeds crit chance.
                return field::setF32(obj, off::kPlayerStealthOff, f);
            case CUBE_STAT_BASE_DAMAGE:
                return field::setF32(obj, off::kPlayerBaseDamageOff, f);
            case CUBE_STAT_SNEAKING:
                // There is no crouch bit; sneaking is the stealth stat (>0 = sneaking).
                return field::setF32(obj, off::kPlayerStealthOff, (i != 0) ? off::kStealthMax : 0.0f);
            case CUBE_STAT_LANTERN:
            {
                uint16_t word = 0;
                if (!mem::read(obj + off::kStateWordOff, word))
                    return false;
                word = (i != 0) ? static_cast<uint16_t>(word | off::kLanternBit)
                                : static_cast<uint16_t>(word & ~off::kLanternBit);
                return mem::write<uint16_t>(obj + off::kStateWordOff, word);
            }
            default: return false;
        }
    }

    bool validateCreature(uint32_t address, uintptr_t& objOut)
    {
        const uintptr_t obj = static_cast<uintptr_t>(address);
        if (!obj || !mem::readable(reinterpret_cast<const void*>(obj), off::kPlayerStructMinSize))
            return false;

        uint32_t vtable = 0;
        if (!mem::read(obj, vtable) || vtable != static_cast<uint32_t>(mem::rebase(off::kCreatureVtable)))
            return false;

        objOut = obj;
        return true;
    }

    bool resolveCreatureOrLocal(uint32_t address, uintptr_t& objOut)
    {
        if (address == 0)
        {
            uintptr_t gc = 0;
            return resolveLocalPlayer(gc, objOut);
        }
        return validateCreature(address, objOut);
    }

    int32_t resolveDisplayName(uintptr_t creature, int32_t typeId, char* out)
    {
        if (field::cstr(creature, off::kPlayerNameOff, off::kPlayerNameIsWide, kMaxNameChars, out))
            return 1;

        catalogNameOr(CUBE_CATALOG_SPECIES, typeId, out, CUBE_PLAYER_NAME_MAX, "species");
        return 1;
    }

}
