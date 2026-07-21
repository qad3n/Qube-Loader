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
#include <psapi.h>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <string>

namespace
{

    constexpr char kLoadBanner[] = "=====================================================";
    constexpr char kEnvCategory[] = "env";
    constexpr int kEjectKey = VK_END;
    constexpr int kPollIntervalMs = 32;
    constexpr SHORT kKeyDownMask = static_cast<SHORT>(0x8000);
    constexpr int kMaxModules = 1024;

    // Substrings (lowercased) of module names that hook or replace d3d9: a proxy d3d9, translation
    // layers, and in-game overlays. These change the device vtable and are the usual cause of an
    // overlay crash or no-show on a given machine, so a support log should name any that are present.
    const char* const kInterposerModules[] = {
        "dxvk", "d3d9on12", "dgvoodoo", "reshade", "rtss", "gameoverlayrenderer", "discord",
        "nvspcap", "specialk", "fraps", "bandicam", "obs-hook", "amdxx", "gfsdk"
    };

    HMODULE g_self = nullptr;

    void toLower(char* s)
    {
        for (; *s; ++s)
            *s = static_cast<char>(tolower(static_cast<unsigned char>(*s)));
    }

    // Reports which d3d9.dll is loaded (a copy in the game folder rather than the system one is a
    // proxy/wrapper) and lists any injected wrappers/overlays. This is the environment detail that
    // actually explains a d3d overlay crash; OS build is kept because fullscreen/DWM behavior varies
    // by Windows version. No user or machine names. Info level so it is always in the log.
    void logEnvironment()
    {
        typedef LONG(WINAPI* RtlGetVersionFn)(LPOSVERSIONINFOEXW);
        OSVERSIONINFOEXW osv = {};
        osv.dwOSVersionInfoSize = sizeof(osv);
        HMODULE ntdll = GetModuleHandleA("ntdll.dll");
        RtlGetVersionFn rtlGetVersion = ntdll ? reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion"))) : nullptr;
        if (rtlGetVersion)
            rtlGetVersion(&osv);
        LOGC(Info, kEnvCategory, "OS Windows %lu.%lu build %lu",
             static_cast<unsigned long>(osv.dwMajorVersion), static_cast<unsigned long>(osv.dwMinorVersion),
             static_cast<unsigned long>(osv.dwBuildNumber));

        // Which d3d9.dll: a local copy (game folder) instead of the system one is a wrapper/proxy.
        HMODULE d3d9 = GetModuleHandleA("d3d9.dll");
        if (d3d9 == nullptr)
            LOGC(Info, kEnvCategory, "d3d9.dll not loaded yet");
        else
        {
            char path[MAX_PATH] = {};
            char sysDir[MAX_PATH] = {};
            GetModuleFileNameA(d3d9, path, MAX_PATH);
            const UINT sysLen = GetSystemDirectoryA(sysDir, MAX_PATH);
            const bool isSystem = sysLen > 0 && _strnicmp(path, sysDir, sysLen) == 0;
            LOGC(Info, kEnvCategory, "d3d9.dll: %s (%s)", path, isSystem ? "system" : "LOCAL COPY - wrapper/proxy in game folder");
        }

        // Injected d3d wrappers / overlays present in the process.
        HMODULE mods[kMaxModules];
        DWORD needed = 0;
        std::string hits;
        if (EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed))
        {
            const int count = static_cast<int>((needed < sizeof(mods) ? needed : sizeof(mods)) / sizeof(HMODULE));
            for (int i = 0; i < count; ++i)
            {
                char name[MAX_PATH] = {};
                if (GetModuleBaseNameA(GetCurrentProcess(), mods[i], name, sizeof(name)) == 0)
                    continue;
                char lower[MAX_PATH];
                lstrcpynA(lower, name, sizeof(lower));
                toLower(lower);
                for (const char* needle : kInterposerModules)
                {
                    if (std::strstr(lower, needle) == nullptr)
                        continue;
                    if (!hits.empty())
                        hits += ", ";
                    hits += name;
                    break;
                }
            }
        }
        LOGC(Info, kEnvCategory, "d3d wrappers/overlays injected: %s", hits.empty() ? "none detected" : hits.c_str());
    }

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
        logEnvironment();

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
            const std::size_t modCount = modloader::install(dir, cfg.overlay);

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
