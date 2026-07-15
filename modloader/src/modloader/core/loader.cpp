#include "modloader/core/internal.h"
#include "modloader/core/events.h"
#include "modloader/core/conflict.h"
#include "game/gamehooks/gamehooks.h"
#include "api/api.h"
#include "api/context.h"
#include "core/log.h"
#include "core/paths.h"
#include "util/guard.h"
#include "cube_sdk.h"

#include <windows.h>
#include <cstddef>
#include <memory>
#include <string>

// Mod discovery + loading: scan <dllDir>/mods, LoadLibrary each, resolve CubeMod_Init, hand it a
// CubeApi, and record the live mod in loadedMods().
namespace modloader
{
    namespace
    {
        constexpr char kDllPattern[] = "\\*.dll";
        constexpr char kDllSuffix[] = ".dll";
        constexpr char kInitExport[] = "CubeMod_Init";
        constexpr char kShutdownExport[] = "CubeMod_Shutdown";

        std::string stemOf(const char* fileName)
        {
            std::string name = fileName;
            const size_t dot = name.rfind(kDllSuffix);

            if (dot != std::string::npos && dot == name.size() - (sizeof(kDllSuffix) - 1))
                name.erase(dot);

            return name;
        }

        // How many already-loaded mods report this exact name (0 = unique so far).
        int32_t countLoadedNamed(const std::string& name)
        {
            int32_t count = 0;
            for (const std::unique_ptr<LoadedMod>& mod : loadedMods())
            {
                if (mod->name == name)
                    ++count;
            }

            return count;
        }

        void loadOne(const std::string& fullPath, const std::string& stem)
        {
            HMODULE module = LoadLibraryA(fullPath.c_str());
            if (!module)
            {
                LOGC(Warn, kCategory, "failed to load %s (error %lu)", stem.c_str(),
                     static_cast<unsigned long>(GetLastError()));
                return;
            }

            CubeModInitFn init = reinterpret_cast<CubeModInitFn>(
                reinterpret_cast<void*>(GetProcAddress(module, kInitExport)));
            if (!init)
            {
                LOGC(Warn, kCategory, "%s has no %s export; not a cube mod, skipping", stem.c_str(), kInitExport);
                FreeLibrary(module);
                return;
            }

            std::unique_ptr<LoadedMod> mod(new LoadedMod());
            mod->module = module;
            mod->context.category = stem;

            if (!api::fill(mod->context.api))
            {
                LOGC(Warn, kCategory, "%s: CubeApi failed to initialize completely (see api errors above); skipping",
                     stem.c_str());
                FreeLibrary(module);
                return;
            }

            LOGC(Debug, kCategory, "%s: init export found; handed CubeApi (ABI v%u)", stem.c_str(),
                 static_cast<unsigned>(mod->context.api.abiVersion));

            CubeModInfo* info = nullptr;
            const std::string initLabel = std::string("mod '") + stem + "' init";

            const bool initOk = guard::tryRun(initLabel.c_str(), &mod->context.api, [&]()
            {
                info = init(&mod->context.api);
            });

            if (!initOk)
            {
                LOGC(Warn, kCategory, "%s: CubeMod_Init threw; unloading (its handlers were dropped)", stem.c_str());
                modloader::events::unsubscribeOwner(&mod->context.api);
                game::gamehooks::unsubscribeOwner(&mod->context.api);
                FreeLibrary(module);
                return;
            }

            if (!info || !info->name)
            {
                LOGC(Warn, kCategory, "%s: CubeMod_Init returned no info; unloading", stem.c_str());
                modloader::events::unsubscribeOwner(&mod->context.api);
                game::gamehooks::unsubscribeOwner(&mod->context.api);
                FreeLibrary(module);
                return;
            }

            mod->name = info->name;
            if (info->structSize >= offsetof(CubeModInfo, priority) + sizeof(info->priority))
                mod->context.priority = info->priority;
            const int32_t priorSameName = countLoadedNamed(info->name);
            if (priorSameName > 0)
            {
                mod->context.category = std::string(info->name) + "#" + std::to_string(priorSameName + 1);
                conflict::error("two mods are named '%s'; loading both (as '%s' and '%s'). Logs are ambiguous and the game may crash or misbehave - remove one",
                                info->name, info->name, mod->context.category.c_str());
            }
            else
            {
                mod->context.category = info->name;
            }
            mod->shutdown = reinterpret_cast<CubeModShutdownFn>(reinterpret_cast<void*>(GetProcAddress(module, kShutdownExport)));

            LOGC(Info, kCategory, "loaded %s v%s by %s", info->name, info->version ? info->version : "?", info->author ? info->author : "?");
            const std::string handlers = modloader::events::describeOwner(&mod->context.api);
            LOGC(Debug, kCategory, "  '%s': shutdown export %s; event handlers: %s", info->name, mod->shutdown ? "present" : "absent", handlers.empty() ? "none (inert unless it polls the API each frame)" : handlers.c_str());
            loadedMods().push_back(std::move(mod));
        }

    }

    void scan(const std::string& modsDir)
    {
        const std::string pattern = modsDir + kDllPattern;
        WIN32_FIND_DATAA entry = {};
        HANDLE find = FindFirstFileA(pattern.c_str(), &entry);
        if (find == INVALID_HANDLE_VALUE)
            return;

        do
        {
            if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;

            const std::string full = paths::join(modsDir, entry.cFileName);
            loadOne(full, stemOf(entry.cFileName));
        }
        while (FindNextFileA(find, &entry));

        FindClose(find);
    }

}
