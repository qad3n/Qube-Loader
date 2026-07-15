#include "cube_mod.hpp"
#include "mod_context.h"
#include "overlay.h"
#include "features/cheats.h"
#include "features/game_events.h"
#include "features/game_hooks.h"
#include "features/health_history.h"

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
    mod.log.info("example_mod: init; menu on INSERT/DELETE, listening for game events");

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
}
