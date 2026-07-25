#include "game/gamehooks/builtin/builtin.h"
#include "game/gamehooks/gamehooks.h"
#include "game/offsets.h"
#include "util/guard.h"
#include "util/inflight.h"

#include <atomic>
#include <cstdint>

// Hand written detour behind CUBE_HOOK_ATTACK_DAMAGE (Global::stat_calcAttackDamage @ 00444db0);
// __thiscall == __fastcall with a dummy edx on mingw.
namespace game::gamehooks
{
    namespace
    {
        // Compute the attacker's outgoing damage, returns float in ST0.
        typedef float(__fastcall* AttackDamageFn)(void* self, void* edx);
        AttackDamageFn g_attackDamageOrig = nullptr;
        std::atomic<bool> g_attackDamageActive{false};

        float __fastcall attackDamageDetour(void* self, void* edx)
        {
            barrier::InFlight inflight(builtin::inFlight(CUBE_HOOK_ATTACK_DAMAGE));
            const float real = g_attackDamageOrig ? g_attackDamageOrig(self, edx) : 0.0f;
            if (!g_attackDamageActive.load(std::memory_order_acquire))
                return real;

            CubeHookCall call = {};
            call.structSize = sizeof(CubeHookCall);
            call.hook = CUBE_HOOK_ATTACK_DAMAGE;
            call.self = reinterpret_cast<uint32_t>(self);
            call.argCount = 0;
            call.returnF = real;

            guard::tryRunLoader("attackdamage dispatch", [&]() { dispatchBuiltin(CUBE_HOOK_ATTACK_DAMAGE, call); });
            return call.returnF;
        }

        struct Registrar
        {
            Registrar()
            {
                builtin::registerDef(builtin::Def{CUBE_HOOK_ATTACK_DAMAGE, off::kStatCalcAttackDamageFn,
                    reinterpret_cast<void*>(&attackDamageDetour),
                    reinterpret_cast<void**>(&g_attackDamageOrig), &g_attackDamageActive});
            }
        };
        const Registrar g_registrar;
    }
}
