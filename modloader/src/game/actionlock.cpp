#include "game/actionlock.h"
#include "game/offsets.h"
#include "core/mem.h"

#include <cstdint>

namespace game::actionlock
{
    namespace
    {
        // Health drop that counts as "took damage" this frame. Health only rises from regen between
        // frames, so any real decrease past this float noise margin means a hit, not a roll.
        constexpr float kDamageEpsilon = 0.5f;
        constexpr uint8_t kGroundOrFluidMask = off::kGroundContactBit | off::kContactSwimBit;

        struct State
        {
            uintptr_t player = 0;
            bool hasPrev = false;
            int32_t prevTimer = 0;
            float prevHealth = 0.0f;
            bool prevGrounded = false;
            Cause cause = Cause::None;
        };

        State g_state;
    }

    void sample(uintptr_t localPlayer)
    {
        if (!localPlayer)
        {
            g_state = State{};
            return;
        }

        if (localPlayer != g_state.player)
        {
            g_state = State{};
            g_state.player = localPlayer;
        }

        float health = 0.0f;
        int32_t timer = 0;
        uint8_t contact = 0;
        if (!mem::read(localPlayer + off::kPlayerHealthOff, health) ||
            !mem::read(localPlayer + off::kPlayerHitStunOff, timer) ||
            !mem::read(localPlayer + off::kPlayerContactFlagsOff, contact))
        {
            return;
        }

        const bool grounded = (contact & kGroundOrFluidMask) != 0;
        const bool risingEdge = timer > 0 && g_state.prevTimer <= 0;

        if (risingEdge)
        {
            const bool tookDamage = g_state.hasPrev && health < g_state.prevHealth - kDamageEpsilon;
            if (tookDamage)
                g_state.cause = Cause::Stunned;
            else if (g_state.prevGrounded)
                g_state.cause = Cause::Rolling; // self initiated dodge from the ground, no health lost
            else
                g_state.cause = Cause::Stunned; // airborne lock without damage: not a roll, treat as stun
        }
        else if (timer <= 0)
        {
            g_state.cause = Cause::None;
        }

        g_state.prevTimer = timer;
        g_state.prevHealth = health;
        g_state.prevGrounded = grounded;
        g_state.hasPrev = true;
    }

    Cause cause()
    {
        return g_state.cause;
    }

    bool rolling()
    {
        return g_state.cause == Cause::Rolling;
    }

    bool stunned()
    {
        return g_state.cause == Cause::Stunned;
    }

    uintptr_t subject()
    {
        return g_state.player;
    }
}
