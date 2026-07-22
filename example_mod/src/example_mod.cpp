#include "cube_mod.hpp"
#include "example_lib_api.h"
#include "mod_context.h"
#include "menu/menu.h"
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

    constexpr int kDispatchPriority = 20; // higher runs last in every reduce; example_lib runs before it
}

CUBE_MOD("Example Menu Mod", "1.0.0", "cube_mod")
{
    exmod::g_api = mod.raw();

    // Manifest: a stable id keys this mod's services; capabilities gate the powers it uses; dependsOn
    // requires the example_lib companion (and version range) before this mod's READY; requiredAbi is
    // stamped automatically. setPriority orders this mod last in every reduce.
    mod.setId("cube_mod.example");
    mod.dependsOn(exlib::kModId, "1.0.0");
    mod.setPriority(exmod::kDispatchPriority);
    mod.setCapabilities(cube::Capability::RawMem | cube::Capability::Writes |
                        cube::Capability::RawHooks | cube::Capability::Overlay |
                        cube::Capability::Assets);
    mod.setUpdateUrl("https://github.com/cube-world-mods/example_mod");

    mod.log.info("example_mod: init; menu on INSERT/DELETE, listening for game events");

    // storage() holds mod owned save data (this launch counter survives restarts); config() holds
    // user editable settings (see Mod > Persist). Both key on the DLL stem, so they work here in init.
    const int launches = mod.storage().getValue<int>("launches", 0) + 1;
    mod.storage().putValue<int>("launches", launches);
    if (mod.config().getBool("greet_on_load", true))
        mod.log.info("example_mod: launch #%d - %s", launches,
                     mod.config().getString("greeting", "welcome back").c_str());

    // The whole menu: one call. The loader owns the D3D9 hook, the ImGui context + backends, the
    // per frame New/Render, the INSERT toggle, the DPI/scale, device reset recreate and the game input
    // freeze. We just hand it a draw callback and write ImGui inside it. onDraw (not window) because
    // this menu draws its own sidebar + tab layout; a simple mod would use mod.menu().window(title, fn).
    mod.menu().onDraw([] { exmod::menu::draw(); });

    // Plain per frame work (runs on the loader's FRAME event, independent of whether the menu is open).
    cube::EventListener& eventListener = mod.eventListener;
    eventListener.onFrame([]
    {
        exmod::cheats().apply();
        exmod::healthHistory().sample();
    });

    // The event LISTENER observes, the event HOOK intercepts; each owns its wiring in its feature class.
    exmod::gameEvents().install(eventListener);
    exmod::gameHooks().install();
    // Consume example_lib's service, and localize UI strings (see Mod > Services / Mod > Locale).
    exmod::servicesDemo().install(mod);
    exmod::localeDemo().install(mod);
}
