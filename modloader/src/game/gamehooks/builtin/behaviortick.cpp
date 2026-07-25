#include "game/gamehooks/builtin/builtin.h"
#include "game/gamehooks/gamehooks.h"
#include "game/attackwatch.h"
#include "game/offsets.h"
#include "util/guard.h"
#include "util/inflight.h"

#include <atomic>
#include <cstdint>
#include <cstring>

// Hand written detour behind CUBE_HOOK_AI_BEHAVIOR_TICK (cube::CombatBehavior::vfunc_0 @ 0042cb20);
// __thiscall == __fastcall with a dummy edx on mingw.
namespace game::gamehooks
{
    namespace
    {
        // The per tick AI behavior update: drives cooldowns, aiState, target distance, deltaTime.
        // `mov [esp+0x30],ecx` @42cb60 reads ECX on entry, `ret 0x10` @42eee3 cleans four stack args.
        typedef void(__fastcall* BehaviorTickFn)(void* self, void* edx, float a0, float a1, int32_t a2, void* world);
        BehaviorTickFn g_tickOrig = nullptr;
        std::atomic<bool> g_tickActive{false};

        void __fastcall behaviorTickDetour(void* self, void* edx, float a0, float a1, int32_t a2, void* world)
        {
            barrier::InFlight inflight(builtin::inFlight(CUBE_HOOK_AI_BEHAVIOR_TICK));
            bool ranOriginal = false;

            if (!g_tickActive.load(std::memory_order_acquire))
            {
                if (g_tickOrig)
                {
                    g_tickOrig(self, edx, a0, a1, a2, world);
                    ranOriginal = true;
                }
            }
            else
            {
                CubeHookCall call = {};
                call.structSize = sizeof(CubeHookCall);
                call.hook = CUBE_HOOK_AI_BEHAVIOR_TICK;
                call.self = reinterpret_cast<uint32_t>(self);
                call.argf[0] = a0;
                call.argf[1] = a1;
                call.argi[2] = a2;
                call.argi[3] = static_cast<int32_t>(reinterpret_cast<uintptr_t>(world));
                call.argCount = 4;

                int32_t cancel = 0;
                guard::tryRunLoader("behaviortick dispatch", [&]() { cancel = dispatchBuiltin(CUBE_HOOK_AI_BEHAVIOR_TICK, call); });
                // cancel skips this creature's whole AI update for the tick (it freezes in place).
                if (!cancel && g_tickOrig)
                {
                    g_tickOrig(reinterpret_cast<void*>(static_cast<uintptr_t>(call.self)), edx,
                        call.argf[0], call.argf[1], call.argi[2],
                        reinterpret_cast<void*>(static_cast<uintptr_t>(call.argi[3])));
                    ranOriginal = true;
                }
            }

            // The tick sets the action byte; sample the local player's action right after it ran so a
            // sub frame attack/shot/ability pulse is caught on the game thread (the render poll misses
            // it). ECX is the CombatBehavior, not the Creature, and the decomp names the first stack
            // arg "creature" while typing it float, so offer both candidates by raw bits;
            // onBehaviorTick ignores anything that is not the cached local player, so the wrong one is
            // a cheap no op.
            if (ranOriginal)
            {
                uint32_t a0Bits = 0;
                std::memcpy(&a0Bits, &a0, sizeof(a0Bits));
                guard::tryRunLoader("attackwatch", [&]()
                {
                    attackwatch::onBehaviorTick(a0Bits);
                    attackwatch::onBehaviorTick(reinterpret_cast<uint32_t>(self));
                });
            }
        }

        struct Registrar
        {
            Registrar()
            {
                builtin::registerDef(builtin::Def{CUBE_HOOK_AI_BEHAVIOR_TICK, off::kCombatBehaviorTickFn,
                    reinterpret_cast<void*>(&behaviorTickDetour),
                    reinterpret_cast<void**>(&g_tickOrig), &g_tickActive});
            }
        };
        const Registrar g_registrar;
    }
}
