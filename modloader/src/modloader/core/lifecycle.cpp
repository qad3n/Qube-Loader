#include "modloader/modloader.h"
#include "modloader/core/internal.h"
#include "modloader/core/events.h"
#include "modloader/game/gameevents.h"
#include "modloader/core/writeguard.h"
#include "game/gamehooks/gamehooks.h"
#include "game/selection.h"
#include "game/pickup.h"
#include "hooks/render_dispatch.h"
#include "hooks/d3d9_hook.h"
#include "hooks/dinput.h"
#include "hooks/input_block.h"
#include "core/log.h"
#include "core/paths.h"
#include "util/guard.h"
#include "cube_sdk.h"

#include <windows.h>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

// The loader core: orchestrate discovery + reporting, wire the render dispatch, and unload cleanly.
namespace modloader
{
    namespace
    {
        constexpr char kModsDirName[] = "mods";

        std::vector<std::unique_ptr<LoadedMod>> g_mods;
        hooks::render::Token g_renderToken = hooks::render::kInvalidToken;

        // Arm the loader's game hooks now that at least one mod is present. Every one is a transparent
        // pass-through: it changes vanilla behavior ONLY when a mod's own callback acts (subscribes and
        // overrides, or opens its overlay). The observation reservations (attack/crit) stay pass-through
        // until a mod hooks them; selection/pickup capture is read-only; the input/DI freeze is inert
        // until a mod calls input.setBlocked. With no mod loaded none of these are installed at all.
        void installModHooks()
        {
            game::gamehooks::armAttackWatch();
            game::gamehooks::armCritCounter();
            game::selection::install();
            game::pickup::install();
            hooks::input_block::install();
            hooks::dinput::install();
        }

        // Reverse of installModHooks. Detour removals must precede MinHook shutdown (done by the caller
        // after remove()); each remove no-ops if its install never ran.
        void removeModHooks()
        {
            game::pickup::remove();
            game::selection::remove();
            hooks::dinput::remove();
            hooks::input_block::remove();
        }

    }

    std::vector<std::unique_ptr<LoadedMod>>& loadedMods()
    {
        return g_mods;
    }

    std::size_t install(const std::string& dllDir)
    {
        const std::string modsDir = paths::join(dllDir, kModsDirName);
        LOGC(Debug, kCategory, "scanning for mods in %s", modsDir.c_str());
        if (!CreateDirectoryA(modsDir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            LOGC(Warn, kCategory, "cannot create mods dir %s (error %lu); no mods loaded",
                 modsDir.c_str(), static_cast<unsigned long>(GetLastError()));

            return 0;
        }

        scan(modsDir);

        if (g_mods.empty())
        {
            LOGC(Info, kCategory, "no mods found in %s", modsDir.c_str());
            return 0;
        }

        // Report what mods attached to and warn about shared hooks, before the loader's own internal
        // subscriptions below would muddy the index.
        reportCompatibility();

        // Arm the loader's game hooks (all pass-through until a mod acts) now that a mod is present.
        installModHooks();

        // Attribute and detect contended game-memory writes across mods for the whole session.
        writeguard::install();

        // Deliver STARTUP on this (mod) thread and drain it BEFORE arming the render dispatch, so a
        // FRAME cannot reenter a mod on the render thread while its STARTUP handler still runs.
        gameevents::emitLifecycle(CUBE_EVENT_STARTUP);

        hooks::d3d9::Callbacks callbacks;
        callbacks.onRender = &gameevents::onFrame;
        callbacks.onDeviceReset = &gameevents::onDeviceReset;
        callbacks.onWndProc = &gameevents::onWndProc;
        g_renderToken = hooks::render::subscribe(callbacks);

        LOGC(Debug, kCategory, "subscribed to render dispatch; forwarding FRAME/DEVICE_RESET/WNDPROC to mods");
        LOGC(Info, kCategory, "%zu mod(s) loaded and started", g_mods.size());

        return g_mods.size();
    }

    void remove()
    {
        // Stop per-frame delivery first so no mod code runs on the render thread while we free the DLLs.
        hooks::render::unsubscribe(g_renderToken);
        g_renderToken = hooks::render::kInvalidToken;

        if (!g_mods.empty())
            gameevents::emitLifecycle(CUBE_EVENT_SHUTDOWN);

        for (size_t i = g_mods.size(); i > 0; --i)
        {
            LoadedMod* mod = g_mods[i - 1].get();
            if (mod->shutdown)
            {
                const std::string shutdownLabel = std::string("mod '") + mod->name + "' shutdown";
                guard::tryRun(shutdownLabel.c_str(), &mod->context.api, [&]()
                {
                    mod->shutdown();
                });
            }

            modloader::events::unsubscribeOwner(&mod->context.api);
            game::gamehooks::unsubscribeOwner(&mod->context.api);

            FreeLibrary(mod->module);
            LOGC(Debug, kCategory, "unloaded %s", mod->name.c_str());
        }

        // Remove the loader's own game hooks (detours + input/DI IAT). No-ops if installModHooks never
        // ran (no mods). The IMPACT/CRIT reservations are torn down by gamehooks::shutdown afterward.
        removeModHooks();

        g_mods.clear();
        modloader::events::clear();
        game::gamehooks::clear();
        writeguard::remove();
    }

}
