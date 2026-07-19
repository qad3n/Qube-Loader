#include "cube_mod.hpp"
#include "mod_context.h"
#include "overlay.h"
#include "features/cheats.h"
#include "features/game_events.h"
#include "features/game_hooks.h"
#include "features/health_history.h"
#include "features/services_demo.h"
#include "features/locale_demo.h"

namespace exmod
{
    const CubeApi* g_api = nullptr;

    void logLine(CubeLogLevel level, const char* message)
    {
        if (g_api)
            g_api->log.write(g_api, level, message);
    }
}

// The example mod is a thin consumer of the modloader API. It registers handlers and delegates all
// logic to the feature classes (event observing, hook interception, per frame cheats); the menu is a
// pure view over that state (see menu/ + features/).

CUBE_MOD("Example Menu Mod", "1.0.0", "cube_mod")
{
    exmod::g_api = mod.raw();

    // Manifest: a stable id keys this mod's config/storage/services; capabilities document what it
    // uses. requiredAbi is stamped automatically by CUBE_MOD. A mod with dependencies would also call
    // mod.dependsOn("other.mod.id", "1.0") to require another mod be present (and in range) first.
    mod.setId("cube_mod.example");
    mod.setCapabilities(cube::Capability::RawMem | cube::Capability::Writes |
                        cube::Capability::RawHooks | cube::Capability::Overlay);

    mod.log.info("example_mod: init; menu on INSERT/DELETE, listening for game events");

    // Persistence demo (ABI 21): storage() holds mod-owned save data (this launch counter survives
    // restarts); config() holds user-editable settings (see the Mod > Persist tab). Both key on this
    // mod's DLL stem (example_mod), not the id above, so they work here in init before the id resolves.
    const int launches = mod.storage().getValue<int>("launches", 0) + 1;
    mod.storage().putValue<int>("launches", launches);
    if (mod.config().getBool("greet_on_load", true))
        mod.log.info("example_mod: launch #%d - %s", launches,
                     mod.config().getString("greeting", "welcome back").c_str());

    // Rendering: the loader hands us the device/window through these events; the overlay owns all
    // ImGui (see overlay.cpp).
    cube::EventListener& eventListener = mod.eventListener;
    eventListener.onRaw(cube::Event::Frame, [](cube::EventArgs& a) { exmod::overlay::onFrame(&a); });
    eventListener.onRaw(cube::Event::DeviceReset, [](cube::EventArgs& a) { exmod::overlay::onDeviceReset(&a); });
    eventListener.onRaw(cube::Event::WndProc, [](cube::EventArgs& a) { exmod::overlay::onWndProc(&a); });
    eventListener.onUnload([] { exmod::overlay::onShutdown(nullptr); });

    // Per frame behavior lives in the feature classes, applied every frame independent of the menu.
    eventListener.onFrame([]
    {
        exmod::cheats().apply();
        exmod::healthHistory().sample();
    });

    // Two separate systems (see cube_mod.hpp): the event LISTENER observes, the event HOOK intercepts.
    // Each owns its wiring + state in its feature class; the menu only views/toggles it.
    exmod::gameEvents().install(eventListener);
    exmod::gameHooks().install();

    // Inter-mod ecosystem demo (ABI 22): publish a service, resolve it at onReady, exchange a directed
    // message. Self-targeted here (one DLL), but the round-trip is identical across two mods.
    exmod::servicesDemo().install(mod);

    // Localization demo (ABI 23): translate keys against lang/example_mod/<locale>.ini and switch the
    // active locale live (see the Mod > Locale tab). Logs a translated greeting at load.
    exmod::localeDemo().install(mod);
}
