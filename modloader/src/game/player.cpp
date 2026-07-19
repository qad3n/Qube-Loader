#include "game/player.h"
#include "game/creature.h"
#include "game/gamecontroller.h"
#include "game/actionlock.h"
#include "game/offsets.h"
#include "util/field.h"
#include "game/catalog.h"
#include "core/mem.h"
#include "util/math.h"

#include <cstdint>
#include <cstdio>

namespace game
{
    namespace
    {
        constexpr char kUnknownClass[] = "Unknown";

        CubeMovement resolveMovement(uint8_t contact, uint16_t stateFlags, bool sneaking)
        {
            if ((stateFlags & off::kClimbEnableBit) && (contact & off::kContactClimbBit))
                return CUBE_MOVE_CLIMBING;
            if (contact & off::kContactSwimBit)
                return CUBE_MOVE_SWIMMING;
            if (contact & off::kGroundContactBit)
                return sneaking ? CUBE_MOVE_SNEAKING : CUBE_MOVE_GROUNDED;
            if (stateFlags & off::kGlideActiveBit)
                return CUBE_MOVE_GLIDING; // airborne with the hang-glider deployed (see kGlideActiveBit)
            return CUBE_MOVE_AIRBORNE; // off the ground and not climbing/swimming/gliding
        }

        CubeAction resolveAction(uintptr_t creature, uint8_t action, bool combat)
        {
            if (action == off::kActionBlock)
                return CUBE_ACTION_BLOCKING;
            if (!combat)
                return (action == off::kActionIdle) ? CUBE_ACTION_IDLE : CUBE_ACTION_UNKNOWN;

            // A staff-equipped combat action is a spell cast; anything else is an attack/shot.
            uint8_t weaponSub = 0;
            mem::read(creature + off::kWeaponSubtypeOff, weaponSub);
            return (weaponSub == off::kWeaponSubStaff) ? CUBE_ACTION_CASTING : CUBE_ACTION_ATTACKING;
        }

        void readActionState(uintptr_t base, CubePlayer& out)
        {
            bool any = false;
            uint8_t action = 0;

            if (off::kPlayerActionOff && mem::read(base + off::kPlayerActionOff, action))
            {
                const bool combat = isCombatActionId(base, action);
                out.actionId = static_cast<int32_t>(action);
                mem::read(base + off::kPlayerActionElapsedOff, out.actionElapsedMs);
                out.action = resolveAction(base, action, combat);
                out.knockedDown = (action == off::kActionKnockdown) ? 1 : 0;
                any = true;
            }

            // "Sneaking" is the stealth stat (there is no crouch bit); the lantern is a render flag.
            float stealth = 0.0f;
            field::f32(base, off::kPlayerStealthOff, stealth);
            out.stealth = stealth;
            out.sneaking = (stealth > 0.0f) ? 1 : 0;

            uint16_t stateWord = 0;
            if (mem::read(base + off::kStateWordOff, stateWord))
                out.lantern = (stateWord & off::kLanternBit) ? 1 : 0;

            uint8_t contact = 0;
            if (off::kPlayerContactFlagsOff && mem::read(base + off::kPlayerContactFlagsOff, contact))
            {
                uint16_t stateFlags = 0;
                mem::read(base + off::kPlayerStateFlagsOff, stateFlags);
                out.movement = resolveMovement(contact, stateFlags, out.sneaking != 0);
                out.onGround = (out.movement == CUBE_MOVE_GROUNDED || out.movement == CUBE_MOVE_SNEAKING) ? 1 : 0;
                any = true;
            }

            out.hasState = any ? 1 : 0;
        }

        void readVitals(uintptr_t base, CubePlayer& out)
        {
            field::f32(base, off::kPlayerManaOff, out.mana);
            field::f32(base, off::kPlayerStaminaOff, out.stamina);
            out.manaPercent = out.mana * off::kPercentScale;
            out.staminaPercent = out.stamina * off::kPercentScale;
            field::i32(base, off::kPlayerCoinsOff, out.coins);
            readCreatureVelocity(base, out.velX, out.velY, out.velZ);
            out.speed = mathutil::magnitude2(out.velX, out.velY);
        }

    }

    bool readPlayer(CubePlayer& out)
    {
        uintptr_t gc = 0;
        uintptr_t obj = 0;

        if (!resolveLocalPlayer(gc, obj))
            return false;

        actionlock::sample(obj);

        out.address = static_cast<uint32_t>(obj);
        field::f32(obj, off::kPlayerHealthOff, out.health);
        out.alive = (out.health > kDeadHealth) ? 1 : 0;
        field::i32(obj, off::kPlayerLevelOff, out.level);
        field::u32(obj, off::kPlayerXpOff, out.xp);
        field::i32(obj, off::kPlayerTypeOff, out.type);
        field::byteI32(obj, off::kPlayerClassOff, out.classForm);
        const char* className = catalogName(CUBE_CATALOG_CLASS, out.classForm);
        std::snprintf(out.className, sizeof(out.className), "%s", className ? className : kUnknownClass);
        field::byteI32(obj, off::kPlayerSpecOff, out.spec);
        out.hasName = field::cstr(obj, off::kPlayerNameOff, off::kPlayerNameIsWide, kMaxNameChars, out.name) ? 1 : 0;

        if (off::kPlayerPosXOff)
        {
            readCreaturePosition(obj, out.x, out.y, out.z);
            out.hasPosition = 1;
        }

        readActionState(obj, out);
        if (!out.alive)
            out.action = CUBE_ACTION_DEAD;
        else if (actionlock::rolling())
            out.action = CUBE_ACTION_ROLLING; // roll leaves the action byte idle; the lock classifier owns it
        readVitals(obj, out);

        // Facing: camera/look yaw is on the GameController and stays live airborne.
        float cameraYawDegrees = 0.0f;
        if (mem::read(gc + off::kCamYawOff, cameraYawDegrees))
            out.facing = mathutil::wrapRadians(cameraYawDegrees * mathutil::kDegToRad);

        uint32_t target = 0;
        if (off::kSelectedEntityOff && mem::read(gc + off::kSelectedEntityOff, target))
            out.target = target;
        return true;
    }

    bool addPlayerXp(int32_t amount)
    {
        uintptr_t gc = 0;
        uintptr_t obj = 0;

        if (!resolveLocalPlayer(gc, obj))
            return false;

        uint32_t current = 0;
        if (!mem::read(obj + off::kPlayerXpOff, current))
            return false;

        return field::setI32(obj, off::kPlayerXpOff, static_cast<int32_t>(current) + amount);
    }

    bool teleportPlayer(float x, float y, float z)
    {
        uintptr_t gc = 0;
        uintptr_t obj = 0;

        if (!resolveLocalPlayer(gc, obj))
            return false;
        return setCreaturePosition(obj, x, y, z);
    }

    bool setPlayerStat(int32_t stat, double value)
    {
        uintptr_t gc = 0;
        uintptr_t obj = 0;
        if (!resolveLocalPlayer(gc, obj))
            return false;

        // The player's facing is the stable camera yaw (degrees) on the GC, not body yaw.
        if (stat == CUBE_STAT_FACING)
            return field::setF32(gc, off::kCamYawOff, static_cast<float>(value) / mathutil::kDegToRad);

        return writeCreatureStat(obj, stat, value);
    }

    bool setPlayerName(const char* name)
    {
        uintptr_t gc = 0;
        uintptr_t obj = 0;

        if (!resolveLocalPlayer(gc, obj))
            return false;
        return setCreatureName(obj, name);
    }

}
