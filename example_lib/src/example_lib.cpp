#include "cube_mod.hpp"
#include "example_lib_api.h"

// A minimal headless companion mod: no ImGui, no menu. It makes the inter-mod ecosystem real, so
// example_mod can declare a dependency on it, resolve its service at READY, and message it. Doubles as
// the smallest possible CUBE_MOD template.

namespace
{
    int CUBE_CALL pingImpl(int value)
    {
        return value * 2;
    }

    exlib::PingService g_ping = {&pingImpl};
}

CUBE_MOD("Example Library", "1.0.0", "cube_mod")
{
    mod.setId(exlib::kModId);
    mod.setPriority(exlib::kPriority);

    mod.services().registerService(exlib::kServiceName, exlib::kServiceVersion, &g_ping);
    mod.services().onMessage([](cube::Message& msg)
    {
        if (msg.id() == exlib::kPingMessage && msg.size() >= sizeof(int))
            msg.reply(*static_cast<const int*>(msg.payload()) + 1);
    });

    mod.log.info("example_lib: service '%s' v%u ready", exlib::kServiceName, exlib::kServiceVersion);
}
