#include "game/status.h"
#include "game/creature.h"
#include "game/gamecontroller.h"
#include "game/offsets.h"
#include "util/field.h"
#include "core/mem.h"

#include <cstdint>

namespace game
{
    int32_t listBuffs(CubeBuff* out, int32_t maxCount)
    {
        uintptr_t gc = 0;
        uintptr_t player = 0;
        if (!resolveLocalPlayer(gc, player))
            return 0;

        return listBuffsAt(player, out, maxCount);
    }

    int32_t listBuffsOfAddress(uint32_t address, CubeBuff* out, int32_t maxCount)
    {
        uintptr_t obj = 0;
        if (!validateCreature(address, obj))
            return 0;

        return listBuffsAt(obj, out, maxCount);
    }

    int32_t listBuffsAt(uintptr_t creature, CubeBuff* out, int32_t maxCount)
    {
        if (!out || maxCount <= 0 || !creature)
            return 0;

        const uintptr_t headSlot = creature + off::kBuffListHeadOff;
        uint32_t sentinel = 0;
        if (!mem::read(headSlot, sentinel) || !sentinel)
            return 0; // list empty / uninitialized

        int32_t count = 0;
        int32_t steps = 0;
        uint32_t node = 0;

        if (!mem::read(sentinel + off::kBuffNodeNextOff, node))
            return 0;

        while (node && node != sentinel && count < maxCount && steps < off::kMaxBuffWalk)
        {
            ++steps;
            uint8_t type = 0;
            if (mem::read(node + off::kBuffNodeTypeOff, type))
            {
                CubeBuff& buff = out[count];

                buff.structSize = sizeof(CubeBuff);
                buff.address = node;
                buff.type = static_cast<int32_t>(type);
                buff.magnitude = 0.0f;
                buff.remainingMs = 0;

                mem::read(node + off::kBuffNodeMagnitudeOff, buff.magnitude);
                mem::read(node + off::kBuffNodeDurationOff, buff.remainingMs);

                ++count;
            }

            if (!mem::read(node + off::kBuffNodeNextOff, node))
                break;
        }
        return count;
    }

    bool setBuffField(uint32_t address, int32_t fieldId, double value)
    {
        const uintptr_t node = static_cast<uintptr_t>(address);
        if (!node || !mem::readable(reinterpret_cast<const void*>(node), off::kBuffNodeDurationOff + sizeof(int32_t)))
            return false;

        switch (fieldId)
        {
            case CUBE_BUFF_TYPE:
                return mem::write<uint8_t>(node + off::kBuffNodeTypeOff, static_cast<uint8_t>(value));
            case CUBE_BUFF_MAGNITUDE:
                return mem::write<float>(node + off::kBuffNodeMagnitudeOff, static_cast<float>(value));
            case CUBE_BUFF_DURATION:
                return mem::write<int32_t>(node + off::kBuffNodeDurationOff, static_cast<int32_t>(value));
            default: return false;
        }
    }

    bool readStun(uint32_t address, CubeStun& out)
    {
        uintptr_t obj = 0;
        if (!resolveCreatureOrLocal(address, obj))
            return false;

        out.structSize = sizeof(CubeStun);
        out.address = static_cast<uint32_t>(obj);

        uint8_t action = 0;
        if (mem::read(obj + off::kPlayerActionOff, action))
            out.knockedDown = (action == off::kActionKnockdown) ? 1 : 0;

        int32_t timer = 0;
        field::i32(obj, off::kPlayerHitStunOff, timer);

        out.hitStun = timer;
        out.stunned = (timer > 0) ? 1 : 0;
        out.hitStunPercent = (timer > 0) ? static_cast<float>(timer) / static_cast<float>(off::kHitStunMax) * off::kPercentScale : 0.0f;

        field::f32(obj, off::kKnockbackVelXOff, out.knockbackX);
        field::f32(obj, off::kKnockbackVelYOff, out.knockbackY);

        out.valid = 1;
        return true;
    }

    bool clearStun(uint32_t address)
    {
        uintptr_t obj = 0;
        if (!resolveCreatureOrLocal(address, obj))
            return false;

        bool ok = field::setI32(obj, off::kPlayerHitStunOff, 0);
        uint8_t action = 0;

        if (mem::read(obj + off::kPlayerActionOff, action) && action == off::kActionKnockdown)
            ok = field::setU8(obj, off::kPlayerActionOff, static_cast<uint8_t>(off::kActionIdle)) && ok;

        return ok;
    }

}
