#include "game/gamehooks/builtin/builtin.h"
#include "game/gamehooks/gamehooks.h"
#include "hooks/detour.h"
#include "core/mem.h"
#include "core/log.h"
#include "util/fmt.h"
#include "util/inflight.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace game::gamehooks
{
    namespace
    {
        constexpr char kCategory[] = "gamehooks";

        // Each detour bumps its slot for the whole body; disarmBuiltin drains it after flipping the
        // active gate to pass-through, so the trampoline is never freed mid-call. Indexed by CubeHook.
        std::atomic<int> g_inFlight[CUBE_HOOK_COUNT];

        // Whether each detour's trampoline is physically installed (MinHook). Kept separate from the
        // dispatch gate (Def::active): a detour can be installed yet pass-through (a reservation with
        // no subscriber), so "installed" tracks the hook and "active" tracks whether handlers run.
        std::atomic<bool> g_installed[CUBE_HOOK_COUNT];

        // Function-local (Meyers) so registration from any TU's static init is order-safe.
        std::vector<builtin::Def>& defs()
        {
            static std::vector<builtin::Def> g_defs;
            return g_defs;
        }

        const builtin::Def* defOf(CubeHook hook)
        {
            for (const builtin::Def& def : defs())
            {
                if (def.hook == hook)
                    return &def;
            }
            return nullptr;
        }

        void* rebasePtr(uintptr_t staticAddress)
        {
            return reinterpret_cast<void*>(mem::rebase(staticAddress));
        }
    }

    uint32_t builtinTarget(CubeHook hook)
    {
        const builtin::Def* def = defOf(hook);
        return def ? static_cast<uint32_t>(mem::rebase(def->target)) : 0;
    }

    namespace builtin
    {
        void registerDef(const Def& def)
        {
            defs().push_back(def);
        }

        std::atomic<int>& inFlight(CubeHook hook)
        {
            return g_inFlight[hook];
        }
    }

    bool armBuiltin(CubeHook hook)
    {
        const builtin::Def* def = defOf(hook);

        if (!def)
        {
            LOGC(Warn, kCategory, "no detour for built-in hook #%d", static_cast<int>(hook));
            return false;
        }

        if (g_installed[hook].load())
            return true;

        void* target = rebasePtr(def->target);

        if (!hooks::detour::create(target, def->detour, def->original))
        {
            LOGC(Warn, kCategory, "built-in hook %s failed to arm at 0x%X (static 0x%X)",
                 hookName(hook), fmt::ptr(target), fmt::ptr(reinterpret_cast<void*>(def->target)));
            return false;
        }

        // Installed but NOT active: the detour runs the original untouched (pass-through) until a real
        // subscriber flips the dispatch gate. This keeps an observation reservation vanilla.
        g_installed[hook].store(true, std::memory_order_release);
        LOGC(Debug, kCategory, "armed built-in hook %s at 0x%X (static 0x%X, trampoline 0x%X)", hookName(hook), fmt::ptr(target), fmt::ptr(reinterpret_cast<void*>(def->target)), fmt::ptr(*def->original));
        return true;
    }

    void setBuiltinActive(CubeHook hook, bool on)
    {
        const builtin::Def* def = defOf(hook);
        if (def)
            def->active->store(on, std::memory_order_release);
    }

    void disarmBuiltin(CubeHook hook)
    {
        const builtin::Def* def = defOf(hook);
        if (!def || !g_installed[hook].load())
            return;

        def->active->store(false, std::memory_order_release); // stop dispatching (pass-through) first
        g_installed[hook].store(false, std::memory_order_release);
        barrier::drain(g_inFlight[def->hook], hookName(hook)); // wait out any in-flight call
        hooks::detour::remove(rebasePtr(def->target));

        *def->original = nullptr;
        LOGC(Debug, kCategory, "disarmed built-in hook %s at 0x%X", hookName(hook),
             fmt::ptr(rebasePtr(def->target)));
    }

    void shutdownBuiltin()
    {
        for (const builtin::Def& def : defs())
        {
            if (g_installed[def.hook].load())
                disarmBuiltin(def.hook);
        }
    }
}
