#include "game/gamehooks/builtin/builtin.h"
#include "game/gamehooks/gamehooks.h"
#include "game/gamecontroller.h"
#include "game/combat.h"
#include "game/offsets.h"
#include "util/guard.h"
#include "util/inflight.h"

#include <atomic>
#include <cstdint>

// Hand-written detour behind CUBE_HOOK_CRIT_ROLL; __thiscall == __fastcall with a dummy edx on mingw.
namespace game::gamehooks
{
    namespace
    {
        // Crit roll, returns bool in AL.
        typedef char(__fastcall* CritFn)(void* self, void* edx);
        CritFn g_critOrig = nullptr;
        std::atomic<bool> g_critActive{false};

        char __fastcall critDetour(void* self, void* edx)
        {
            barrier::InFlight inflight(builtin::inFlight(CUBE_HOOK_CRIT_ROLL));
            const char real = g_critOrig ? g_critOrig(self, edx) : 0;
            char result = real; // vanilla roll stands unless a mod handler explicitly overrides it

            if (g_critActive.load(std::memory_order_acquire))
            {
                CubeHookCall call = {};
                call.structSize = sizeof(CubeHookCall);
                call.hook = CUBE_HOOK_CRIT_ROLL;
                call.self = reinterpret_cast<uint32_t>(self);
                call.argCount = 0;
                call.returnI = real ? 1 : 0;

                guard::tryRun("crit dispatch", [&]() { dispatchBuiltin(CUBE_HOOK_CRIT_ROLL, call); });
                if (call.overrideReturn)
                    result = call.returnI ? 1 : 0;
            }

            // Feed the observe-side counter for the LOCAL player's crits whether or not a mod is
            // intercepting, so PLAYER_CRIT / getCrits() mean "the player crit" and not "any creature on
            // the map rolled a crit". Pass-through (no subscriber) still counts; it never alters the roll.
            if (result && game::isLocalPlayerCreature(reinterpret_cast<uint32_t>(self)))
                game::noteCrit();
            return result;
        }

        struct Registrar
        {
            Registrar()
            {
                builtin::registerDef(builtin::Def{CUBE_HOOK_CRIT_ROLL, off::kCritRollFn,
                    reinterpret_cast<void*>(&critDetour),
                    reinterpret_cast<void**>(&g_critOrig), &g_critActive});
            }
        };
        const Registrar g_registrar;
    }
}
