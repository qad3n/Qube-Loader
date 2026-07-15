#include "core/log.h"
#include "core/config.h"
#include "core/crash.h"
#include "core/faultguard.h"
#include "core/paths.h"
#include "game/gamelog.h"
#include "game/game.h"
#include "game/diag.h"
#include "game/gamehooks/gamehooks.h"
#include "hooks/detour.h"
#include "modloader/modloader.h"
#include "util/guard.h"

#include <windows.h>
#include <cstddef>
#include <string>

namespace
{

    constexpr char kLoadBanner[] = "=====================================================";
    constexpr int kEjectKey = VK_END;
    constexpr int kPollIntervalMs = 32;
    constexpr SHORT kKeyDownMask = static_cast<SHORT>(0x8000);

    HMODULE g_self = nullptr;

    void reportLoadStatus(bool gamelogEnabled, bool gamelogOk, std::size_t modCount)
    {
        LOGI("status: gamelog=%s  mods=%zu", gamelogEnabled ? (gamelogOk ? "ok" : "FAILED") : "disabled", modCount);
        LOGI("%s", kLoadBanner);
        LOGI(" cube_mod loader ready (ABI v%u). drop mods into the mods/ folder next to this DLL.",
             static_cast<unsigned>(CUBE_ABI_VERSION));
        LOGI("%s", kLoadBanner);
    }

    std::string selfDir()
    {
        char path[MAX_PATH] = {};
        const DWORD n = GetModuleFileNameA(g_self, path, MAX_PATH);

        if (n == 0 || n >= MAX_PATH)
            return ".";

        return paths::parentDir(path);
    }

    void waitForEject()
    {
        LOGI("loader resident. press END to unload the mod.");
        bool wasDown = false;

        for (;;)
        {
            const bool down = (GetAsyncKeyState(kEjectKey) & kKeyDownMask) != 0;
            if (down && !wasDown)
            {
                LOGD("eject requested (END)");
                return;
            }

            wasDown = down;
            Sleep(kPollIntervalMs);
        }
    }

    void runMod()
    {
        const std::string dir = selfDir();
        const config::Settings cfg = config::load(dir);

        logger::Options opts;
        opts.minLevel = cfg.logLevel;
        opts.logDir = cfg.logDir.c_str();
        opts.console = cfg.console;
        opts.useColor = cfg.color;
        opts.colorMode = cfg.colorMode.c_str();
        logger::init(opts);

        LOGD("cube_mod attached (thread 0x%X)", static_cast<unsigned>(GetCurrentThreadId()));
        config::dump(cfg);

        crash::install(cfg.logDir);
        // Arm mod-fault isolation ahead of the unhandled-exception filter, so a CPU fault in a mod
        // callback disables that mod instead of crashing the game. Config-gated.
        faultguard::install(cfg.faultIsolation);

        // Guard the session so a std::exception mid-init cannot skip teardown and unmap the DLL with
        // hooks still installed. Each teardown step no-ops if its install did not run.
        guard::tryRun("mod session", [&]()
        {
            bool gamelogOk = false;
            if (cfg.captureGameLog)
                gamelogOk = gamelog::install();
            else
                LOGD("game output capture disabled by config");

            // Startup snapshot for RE: base/ASLR slide and the candidate singleton globals.
            game::logModuleInfo();
            game::logCandidateGlobals();

            // Load mods. The loader arms its game hooks (input freeze, DI passthrough, select/pickup
            // capture, attack/crit sampling) ONLY when at least one mod is present, and every one is a
            // transparent pass-through until a mod's own callback acts - so with no mods the game runs
            // exactly as vanilla. See modloader/core/lifecycle.cpp (installModHooks).
            const std::size_t modCount = modloader::install(dir);

            // With no mods loaded, no mod callback will ever run, so mod-fault isolation has nothing to
            // guard. Drop its vectored exception handler now so a mod-less loader leaves zero footprint
            // in the game's exception path (it stays armed through load above to catch a fault in a
            // mod's init). Idempotent with the teardown remove() below.
            if (modCount == 0)
                faultguard::remove();

            reportLoadStatus(cfg.captureGameLog, gamelogOk, modCount);

            // Verbose init diagnostics: report which offsets/chains resolve now (mostly pending on
            // the title screen) and which are deferred. gameevents re-emits it when a world loads.
            game::diag::logKnownGaps();
            game::diag::logResolutionReport();

            waitForEject();
        });

        modloader::remove(); // unsubscribes render + removes the loader's game hooks (select/pickup/DI/input)
        game::gamehooks::shutdown(); // remove any remaining game-function detours (built-in reservations + raw)
        hooks::detour::shutdown(); // single MinHook owner now that all detour users are gone

        LOGD("cube_mod detaching");
        gamelog::remove();
        faultguard::remove();
        crash::remove();
        logger::shutdown();
    }

    DWORD WINAPI modThread(LPVOID)
    {
        guard::tryRun("mod thread", runMod);
        FreeLibraryAndExitThread(g_self, 0);

        return 0;
    }

}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_self = inst;
        DisableThreadLibraryCalls(inst);
        HANDLE thread = CreateThread(nullptr, 0, modThread, nullptr, 0, nullptr);

        if (thread)
            CloseHandle(thread);
    }
    return TRUE;
}
