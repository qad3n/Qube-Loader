#include "game/gamehooks/builtin/builtin.h"
#include "game/gamehooks/gamehooks.h"
#include "game/attackwatch.h"
#include "game/offsets.h"
#include "util/guard.h"
#include "util/inflight.h"

#include <atomic>
#include <cstdint>

// Hand-written detour behind CUBE_HOOK_IMPACT; __thiscall == __fastcall with a dummy edx on mingw.
namespace game::gamehooks
{
    namespace
    {
        // CombatBehavior::vfunc_0 impact/damage/knockback. __thiscall(self, victim, hitCtx, damage, flags), void.
        typedef void(__fastcall* ImpactFn)(void* self, void* edx, void* victim, void* hitCtx,
                                           int32_t damage, uint32_t flags);
        ImpactFn g_impactOrig = nullptr;
        std::atomic<bool> g_impactActive{false};

        void __fastcall impactDetour(void* self, void* edx, void* victim, void* hitCtx, int32_t damage, uint32_t flags)
        {
            barrier::InFlight inflight(builtin::inFlight(CUBE_HOOK_IMPACT));
            bool ranOriginal = false;

            if (!g_impactActive.load(std::memory_order_acquire))
            {
                if (g_impactOrig)
                {
                    g_impactOrig(self, edx, victim, hitCtx, damage, flags);
                    ranOriginal = true;
                }
            }
            else
            {
                CubeHookCall call = {};
                call.structSize = sizeof(CubeHookCall);
                call.hook = CUBE_HOOK_IMPACT;
                call.self = reinterpret_cast<uint32_t>(self);
                call.target = reinterpret_cast<uint32_t>(victim);
                call.argi[0] = damage;
                call.argi[1] = static_cast<int32_t>(reinterpret_cast<uintptr_t>(hitCtx));
                call.argi[2] = static_cast<int32_t>(flags);
                call.argCount = 3;

                int32_t cancel = 0;
                guard::tryRun("impact dispatch", [&]() { cancel = dispatchBuiltin(CUBE_HOOK_IMPACT, call); });
                // cancel negates the hit entirely (no HP loss, stun, or knockback).
                if (!cancel && g_impactOrig)
                {
                    // Re-invoke with every marshalled field so a handler can retarget/rescale, not just argi[0].
                    g_impactOrig(reinterpret_cast<void*>(static_cast<uintptr_t>(call.self)), edx, reinterpret_cast<void*>(static_cast<uintptr_t>(call.target)), reinterpret_cast<void*>(static_cast<uintptr_t>(call.argi[1])), call.argi[0], static_cast<uint32_t>(call.argi[2]));
                    ranOriginal = true;
                }
            }

            // vfunc_0 is the per-tick behavior update that sets the action byte; sample the local player's
            // action right after it ran so a sub-frame attack/shot/ability pulse is caught on the game
            // thread (the render poll misses it). The ticked creature is the first arg (victim); we also
            // pass self in case the convention differs - onBehaviorTick ignores anything that is not the
            // cached local player, so the wrong one is a cheap no-op.
            if (ranOriginal)
            {
                guard::tryRun("attackwatch", [&]()
                {
                    attackwatch::onBehaviorTick(reinterpret_cast<uint32_t>(victim));
                    attackwatch::onBehaviorTick(reinterpret_cast<uint32_t>(self));
                });
            }
        }

        struct Registrar
        {
            Registrar()
            {
                builtin::registerDef(builtin::Def{CUBE_HOOK_IMPACT, off::kImpactFn,
                    reinterpret_cast<void*>(&impactDetour),
                    reinterpret_cast<void**>(&g_impactOrig), &g_impactActive});
            }
        };
        const Registrar g_registrar;
    }
}
