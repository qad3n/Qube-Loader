#include "loader/loader.h"
#include "loader/core/internal.h"
#include "loader/core/modregistry.h"
#include "loader/core/modconfig.h"
#include "loader/core/modstorage.h"
#include "loader/core/modlocale.h"
#include "loader/core/events.h"
#include "loader/core/services.h"
#include "loader/game/gameevents.h"
#include "loader/core/writeguard.h"
#include "game/gamehooks/gamehooks.h"
#include "game/signature.h"
#include "game/assets.h"
#include "game/view.h"
#include "hooks/render_dispatch.h"
#include "hooks/d3d9_hook.h"
#include "hooks/dinput.h"
#include "hooks/input_block.h"
#include "overlay/overlay.h"
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

        // Wire the loader's non game side hooks. NOTHING here patches a game function: every game
        // function detour is now demand driven (a mod subscribes to a detour backed event, calls a pull
        // API, or registers an asset override), so with no mod loaded, or a mod that only reads state,
        // the game binary is untouched. The build guard moved down into each arm path
        // (CaptureDetour::install / armBuiltin / assets::install all call signature::verifyTarget), so a
        // mismatched Cube.exe refuses the arm instead of the whole setup.
        void installModHooks(bool overlayEnabled)
        {
            if (!game::signature::compatibleBuild())
                LOGC(Warn, kCategory, "Cube.exe build mismatch: game-function hooks will refuse to arm (R-select, E-pickup, asset overrides, hook subscriptions); overlay and guarded reads still work");

            // The input freeze (user32 IAT by import name) and DI suspend (system DLL vtable) drive the
            // overlay's input handoff. In safe mode (overlay off) there is no overlay to feed, so skip
            // them too and leave the game's own input untouched.
            if (overlayEnabled)
            {
                hooks::input_block::install();
                hooks::dinput::install();
            }
        }

        // Reverse of installModHooks. Detour removals must precede MinHook shutdown (done by the caller
        // after remove()); each remove no ops if its install never ran. The demand driven game detours
        // come down through eventbacking::releaseAll (from events::clear) and gamehooks::shutdown;
        // assets::remove is here because its holder is the per mod override table, not an event.
        void removeModHooks()
        {
            game::assets::remove();
            hooks::dinput::remove();
            hooks::input_block::remove();
        }

        // Per mod teardown (no list erase): guarded shutdown, drop loader side registrations, free the
        // DLL. Shared by remove() (bulk) and unloadOne (single) so there is exactly one teardown path.
        void teardownMod(LoadedMod* mod)
        {
            if (mod->shutdown)
            {
                const std::string shutdownLabel = std::string("mod '") + mod->name + "' shutdown";
                guard::tryRun(shutdownLabel.c_str(), &mod->context.api, [&]()
                {
                    mod->shutdown();
                });
            }

            // Drop any overlay menus this mod registered (drains an in flight frame) before its code is
            // freed. no op after overlay::shutdown() already cleared the registry (bulk remove()).
            overlay::unregisterOwner(&mod->context.api);
            detachOwner(&mod->context.api);
            FreeLibrary(mod->module);
            LOGC(Debug, kCategory, "unloaded %s", mod->name.c_str());
        }

    }

    std::vector<std::unique_ptr<LoadedMod>>& loadedMods()
    {
        return g_mods;
    }

    void unloadOne(LoadedMod* mod)
    {
        if (!mod)
            return;
        teardownMod(mod);
        for (std::size_t i = 0; i < g_mods.size(); ++i)
        {
            if (g_mods[i].get() == mod)
            {
                g_mods.erase(g_mods.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
        }
    }

    std::size_t install(const std::string& dllDir, bool overlayEnabled)
    {
        const std::string modsDir = paths::join(dllDir, kModsDirName);
        LOGC(Debug, kCategory, "scanning for mods in %s", modsDir.c_str());
        if (!CreateDirectoryA(modsDir.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        {
            LOGC(Warn, kCategory, "cannot create mods dir %s (error %lu); no mods loaded",
                 modsDir.c_str(), static_cast<unsigned long>(GetLastError()));

            return 0;
        }

        // Load the enable/disable + fault strike registry so scan() can skip disabled mods.
        modregistry::load(dllDir);

        // Root the per mod config + storage + locale stores before scan(): a mod may read any of them
        // (a setting, save data, or a translated startup string) in its init.
        modconfig::init(dllDir);
        modstorage::init(dllDir);
        modlocale::init(dllDir);

        scan(modsDir);

        if (g_mods.empty())
        {
            LOGC(Info, kCategory, "no mods found in %s", modsDir.c_str());
            return 0;
        }

        // Resolve dependencies before anything else looks at the mod set: unload mods with unmet hard
        // deps (cascading) and topo rank the survivors' dispatch order. Runs before READY so an unloaded
        // mod never reaches READY nor registers a service.
        resolveDependencies();

        if (g_mods.empty())
        {
            LOGC(Info, kCategory, "all mods unloaded by dependency resolution");
            return 0;
        }

        // Report what mods attached to and warn about shared hooks, before the loader's own internal
        // subscriptions below would muddy the index.
        reportCompatibility();

        // Arm the loader's game hooks (all pass through until a mod acts) now that a mod is present.
        installModHooks(overlayEnabled);

        // Attribute and detect contended game memory writes across mods for the whole session.
        writeguard::install();

        // Deliver STARTUP on this (mod) thread and drain it BEFORE arming the render dispatch, so a
        // FRAME cannot reenter a mod on the render thread while its STARTUP handler still runs.
        gameevents::emitLifecycle(CUBE_EVENT_STARTUP);

        // Every mod is loaded, initialized, and past dependency resolution: READY is the safe point for
        // a mod to resolve another mod's registered service. Same thread/drain discipline as STARTUP.
        gameevents::emitLifecycle(CUBE_EVENT_READY);

        // Arm the render dispatch (lazily installs the D3D9 hook + probe device on first subscribe).
        // Safe mode skips this entirely: no overlay, no probe device, no render driven events.
        if (overlayEnabled)
        {
            hooks::d3d9::Callbacks callbacks;
            callbacks.onRender = &gameevents::onFrame;
            callbacks.onDeviceReset = &gameevents::onDeviceReset;
            callbacks.onWndProc = &gameevents::onWndProc;
            g_renderToken = hooks::render::subscribe(callbacks);
            LOGC(Debug, kCategory, "subscribed to render dispatch; forwarding FRAME/DEVICE_RESET/WNDPROC to mods");

            // The overlay lives in the game's D3D9 swapchain; in EXCLUSIVE fullscreen that window is
            // topmost + non minimizable and loses the device on every alt tab (freeze risk). The D3D9
            // hook forces borderless windowed on the next device Reset. Best effort nudge here so that
            // reset happens promptly instead of waiting for the user's first alt tab: if the game reads
            // as fullscreen, write its display setting to windowed (guarded by a compatible build so the
            // offset is valid). Harmless if the game ignores it, the Reset rewrite still catches every
            // real reset. Only fires when a valid build resolved the setting global.
            CubeDisplay disp = {};
            if (game::signature::compatibleBuild() && game::readDisplay(disp) && disp.fullscreen)
            {
                if (game::setDisplayField(CUBE_DISPLAY_FULLSCREEN, 0))
                    LOGC(Info, kCategory, "nudged game to windowed so the D3D9 hook can force borderless (fixes fullscreen alt-tab/minimize/freeze)");
            }
        }
        else
            LOGC(Warn, kCategory, "safe mode: overlay disabled by config; D3D9 + input hooks not installed (mods still loaded, render-driven events off)");
        LOGC(Info, kCategory, "%zu mod(s) loaded and started", g_mods.size());
        reportVersions();

        return g_mods.size();
    }

    void remove()
    {
        // Stop per frame delivery first so no mod code runs on the render thread while we free the DLLs.
        hooks::render::unsubscribe(g_renderToken);
        g_renderToken = hooks::render::kInvalidToken;

        // Tear the overlay down next: unsubscribes its own render dispatch (draining an in flight frame),
        // releases the input freeze and destroys the shared ImGui context, so no menu draw can run on the
        // render thread once we start freeing mod DLLs below.
        overlay::shutdown();

        if (!g_mods.empty())
            gameevents::emitLifecycle(CUBE_EVENT_SHUTDOWN);

        // Reverse order (mirrors load order) so a later mod tears down before an earlier one it may
        // depend on. teardownMod does not erase; g_mods is cleared in bulk below.
        for (size_t i = g_mods.size(); i > 0; --i)
            teardownMod(g_mods[i - 1].get());

        // Remove the loader's own non game hooks (input/DI IAT) and the asset detour. no ops if
        // installModHooks never ran (no mods). The demand driven detours come down just below, in
        // events::clear (eventbacking::releaseAll) and gamehooks::clear.
        removeModHooks();

        g_mods.clear();
        modloader::events::clear();
        game::gamehooks::clear();
        services::clear();
        writeguard::remove();
    }

}
