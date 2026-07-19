#include "modloader/core/internal.h"
#include "modloader/core/modregistry.h"
#include "modloader/core/events.h"
#include "modloader/core/services.h"
#include "modloader/core/conflict.h"
#include "modloader/core/modassets.h"
#include "modloader/core/owner_name.h"
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

        // How many already-loaded mods share this exact id (0 = unique so far). Id is the stable
        // identity (manifest id, or DLL stem fallback), so a match is a genuine collision.
        int32_t countLoadedId(const std::string& id)
        {
            int32_t count = 0;
            for (const std::unique_ptr<LoadedMod>& mod : loadedMods())
            {
                if (mod->context.id == id)
                    ++count;
            }

            return count;
        }

    }

    void detachOwner(const CubeApi* api)
    {
        modloader::events::unsubscribeOwner(api);
        game::gamehooks::unsubscribeOwner(api);
        services::unregisterOwner(api);
        modassets::dropOwner(ownerStem(api));
    }

    namespace
    {
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
            mod->context.stem = stem;

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
                modregistry::recordFault(stem);
                detachOwner(&mod->context.api);
                FreeLibrary(module);
                return;
            }

            // A clean null return is a deliberate decline (the CUBE_MOD boot returns null when the
            // loader is older than the mod's ABI), not a crash, so it takes no fault strike.
            if (!info)
            {
                conflict::error("mod '%s' declined to load; it is likely built against a newer ABI than v%u, so update the loader",
                                stem.c_str(), static_cast<unsigned>(CUBE_ABI_VERSION));
                detachOwner(&mod->context.api);
                FreeLibrary(module);
                return;
            }

            if (!info->name)
            {
                LOGC(Warn, kCategory, "%s: CubeMod_Init returned info without a name; unloading", stem.c_str());
                detachOwner(&mod->context.api);
                FreeLibrary(module);
                return;
            }

            mod->name = info->name;
            if (info->structSize >= offsetof(CubeModInfo, priority) + sizeof(info->priority))
                mod->context.priority = info->priority;

            // Manifest fields (ABI 20+), each read only if the mod's struct is large enough to hold it.
            const char* declaredId = nullptr;
            if (info->structSize >= offsetof(CubeModInfo, id) + sizeof(info->id))
                declaredId = info->id;
            if (info->structSize >= offsetof(CubeModInfo, requiredAbi) + sizeof(info->requiredAbi))
                mod->requiredAbi = info->requiredAbi;
            if (info->structSize >= offsetof(CubeModInfo, capabilities) + sizeof(info->capabilities))
                mod->context.capabilities = info->capabilities;
            if (info->structSize >= offsetof(CubeModInfo, deps) + sizeof(const CubeModDep*) && info->deps)
            {
                for (const CubeModDep* d = info->deps; d->id; ++d)
                {
                    Dep dep;
                    dep.id = d->id;
                    dep.minVersion = d->minVersion ? d->minVersion : "";
                    dep.hard = d->hard != 0;
                    mod->deps.push_back(dep);
                }
            }
            if (info->structSize >= offsetof(CubeModInfo, updateUrl) + sizeof(info->updateUrl) && info->updateUrl)
                mod->updateUrl = info->updateUrl;
            mod->version = info->version ? info->version : "";
            mod->context.id = (declaredId && declaredId[0]) ? declaredId : stem;

            // ABI gate: reject a mod built against an ABI this loader cannot serve. requiredAbi==0
            // means undeclared (a raw-C mod that did not stamp it) and passes. CUBE_MOD mods always
            // stamp it, so this backs up the mod-side boot check for the raw-ABI path.
            if (mod->requiredAbi != 0 &&
                (mod->requiredAbi < CUBE_MIN_ABI_VERSION || mod->requiredAbi > CUBE_ABI_VERSION))
            {
                conflict::error("mod '%s' was built against ABI v%u, but this loader serves v%u..v%u; rebuild the mod",
                                mod->context.id.c_str(), static_cast<unsigned>(mod->requiredAbi),
                                static_cast<unsigned>(CUBE_MIN_ABI_VERSION), static_cast<unsigned>(CUBE_ABI_VERSION));
                detachOwner(&mod->context.api);
                FreeLibrary(module);
                return;
            }

            // Log category is the stable id on every path (the collision path disambiguates as id#N),
            // so a mod's init-time logs (keyed by stem, the id fallback) and its runtime logs agree.
            const int32_t priorSameId = countLoadedId(mod->context.id);
            if (priorSameId > 0)
            {
                mod->context.category = mod->context.id + "#" + std::to_string(priorSameId + 1);
                conflict::error("two mods share id '%s'; loading both (as '%s' and '%s'). State keyed by id will collide and the game may crash or misbehave - give one a distinct id or remove it",
                                mod->context.id.c_str(), mod->context.id.c_str(), mod->context.category.c_str());
            }
            else
            {
                mod->context.category = mod->context.id;
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

            const std::string stem = stemOf(entry.cFileName);
            if (!modregistry::isEnabled(stem))
            {
                LOGC(Info, kCategory, "%s is disabled in mods.ini; skipping", stem.c_str());
                continue;
            }
            modregistry::noteSeen(stem);

            const std::string full = paths::join(modsDir, entry.cFileName);
            loadOne(full, stem);
        }
        while (FindNextFileA(find, &entry));

        FindClose(find);
    }

}
