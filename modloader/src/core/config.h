#pragma once
// Settings resolved from defaults < cube_mod.ini < environment (see cube_mod.ini.sample).
#include <string>
#include <vector>
#include "core/log.h"

namespace config
{

    struct Settings
    {
        logger::Level logLevel = logger::Level::Debug;
        std::string logDir;
        bool console = true;
        bool color = true;
        // "detect" (default) | "ansi" | "win32" | "none"; force one if colors render wrong.
        std::string colorMode = "detect";
        bool captureGameLog = true;
        // Isolate CPU faults in mod callbacks (disable the mod, not crash the game). See core/faultguard.
        bool faultIsolation = true;
        // ini/env values that failed to parse; reported once by dump() after the logger is up.
        std::vector<std::string> rejected;
    };

    Settings load(const std::string& dllDir);
    void dump(const Settings& s);

}
