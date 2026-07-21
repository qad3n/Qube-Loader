#include "game/gamehooks/builtin/builtin.h"
#include "game/gamehooks/gamehooks.h"
#include "game/offsets.h"
#include "util/guard.h"
#include "util/inflight.h"

#include <atomic>
#include <cstdint>

// Hand-written detour behind CUBE_HOOK_MAX_HEALTH; __thiscall == __fastcall with a dummy edx on mingw.
namespace game::gamehooks
{
    namespace
    {
        // compute-max-health, returns float in ST0.
        typedef float(__fastcall* MaxHealthFn)(void* self, void* edx);
        MaxHealthFn g_maxHealthOrig = nullptr;
        std::atomic<bool> g_maxHealthActive{false};

        float __fastcall maxHealthDetour(void* self, void* edx)
        {
            barrier::InFlight inflight(builtin::inFlight(CUBE_HOOK_MAX_HEALTH));
            const float real = g_maxHealthOrig ? g_maxHealthOrig(self, edx) : 0.0f;
            if (!g_maxHealthActive.load(std::memory_order_acquire))
                return real;

            CubeHookCall call = {};
            call.structSize = sizeof(CubeHookCall);
            call.hook = CUBE_HOOK_MAX_HEALTH;
            call.self = reinterpret_cast<uint32_t>(self);
            call.argCount = 0;
            call.returnF = real;

            guard::tryRunLoader("maxhealth dispatch", [&]() { dispatchBuiltin(CUBE_HOOK_MAX_HEALTH, call); });
            return call.returnF;
        }

        struct Registrar
        {
            Registrar()
            {
                builtin::registerDef(builtin::Def{CUBE_HOOK_MAX_HEALTH, off::kMaxHealthFn,
                    reinterpret_cast<void*>(&maxHealthDetour),
                    reinterpret_cast<void**>(&g_maxHealthOrig), &g_maxHealthActive});
            }
        };
        const Registrar g_registrar;
    }
}
