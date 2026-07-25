#include "game/gamehooks/builtin/builtin.h"
#include "game/gamehooks/gamehooks.h"
#include "game/offsets.h"
#include "util/guard.h"
#include "util/inflight.h"

#include <atomic>
#include <cstdint>

// Hand written detour behind CUBE_HOOK_IMPACT (Global::CombatBehavior_applyMeleeHit @ 00595a60);
// __thiscall == __fastcall with a dummy edx on mingw.
namespace game::gamehooks
{
    namespace
    {
        // The melee hit resolver: applies damage/knockback/threat to the creature in ECX. Ghidra prints
        // it __cdecl, but `mov edi,ecx` @595a8e reads ECX on entry and `ret 0xc` @596c67 cleans the
        // three stack args, so it is __thiscall. `amount` is a FLOAT, not an int.
        typedef void(__fastcall* MeleeHitFn)(void* self, void* edx, void* hitCtx, int32_t attacker, float amount);
        MeleeHitFn g_impactOrig = nullptr;
        std::atomic<bool> g_impactActive{false};

        void __fastcall impactDetour(void* self, void* edx, void* hitCtx, int32_t attacker, float amount)
        {
            barrier::InFlight inflight(builtin::inFlight(CUBE_HOOK_IMPACT));

            if (!g_impactActive.load(std::memory_order_acquire))
            {
                if (g_impactOrig)
                    g_impactOrig(self, edx, hitCtx, attacker, amount);
                return;
            }

            CubeHookCall call = {};
            call.structSize = sizeof(CubeHookCall);
            call.hook = CUBE_HOOK_IMPACT;
            call.self = reinterpret_cast<uint32_t>(self); // ECX: the creature taking the hit
            call.target = static_cast<uint32_t>(attacker);
            call.argf[0] = amount; // damage is a float; marshalling it through argi would be garbage
            call.argi[1] = static_cast<int32_t>(reinterpret_cast<uintptr_t>(hitCtx));
            call.argCount = 2;

            int32_t cancel = 0;
            guard::tryRunLoader("impact dispatch", [&]() { cancel = dispatchBuiltin(CUBE_HOOK_IMPACT, call); });
            // cancel negates the hit entirely (no HP loss, stun, or knockback).
            if (cancel || !g_impactOrig)
                return;

            // Re invoke with every marshalled field so a handler can retarget/rescale, not just argf[0].
            g_impactOrig(reinterpret_cast<void*>(static_cast<uintptr_t>(call.self)), edx,
                reinterpret_cast<void*>(static_cast<uintptr_t>(call.argi[1])),
                static_cast<int32_t>(call.target), call.argf[0]);
        }

        struct Registrar
        {
            Registrar()
            {
                builtin::registerDef(builtin::Def{CUBE_HOOK_IMPACT, off::kApplyMeleeHitFn,
                    reinterpret_cast<void*>(&impactDetour),
                    reinterpret_cast<void**>(&g_impactOrig), &g_impactActive});
            }
        };
        const Registrar g_registrar;
    }
}
