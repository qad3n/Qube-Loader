#include "api/api.h"
#include "api/bridge.h"
#include "cube_sdk.h"

#include <cstddef>

namespace modloader::api
{
    namespace
    {

        // Null entry = a fillX() forgot to wire one; catch it at load, not first use.
        static_assert(sizeof(void (*)()) == sizeof(void*), "function/object pointer size mismatch");

        struct SubApiSpan
        {
            const char* name;
            const void* const* entries;
            std::size_t count;
        };

        template <typename T>
        SubApiSpan spanOf(const char* name, const T& sub)
        {
            return SubApiSpan{name, reinterpret_cast<const void* const*>(&sub), sizeof(T) / sizeof(void*)};
        }

        // Returns the count of unwired entry points, logging each gap.
        int validate(const CubeApi& api)
        {
            const SubApiSpan spans[] =
            {
                spanOf("log", api.log),
                spanOf("mem", api.mem),
                spanOf("events", api.events),
                spanOf("player", api.player),
                spanOf("combat", api.combat),
                spanOf("items", api.items),
                spanOf("skills", api.skills),
                spanOf("status", api.status),
                spanOf("world", api.world),
                spanOf("pet", api.pet),
                spanOf("entities", api.entities),
                spanOf("camera", api.camera),
                spanOf("display", api.display),
                spanOf("audio", api.audio),
                spanOf("session", api.session),
                spanOf("ui", api.ui),
                spanOf("catalog", api.catalog),
                spanOf("input", api.input),
                spanOf("selection", api.selection),
                spanOf("pickup", api.pickup),
                spanOf("hooks", api.hooks),
            };
            const std::size_t subCount = sizeof(spans) / sizeof(spans[0]);

            int total = 0;
            int missing = 0;
            for (const SubApiSpan& span : spans)
            {
                for (std::size_t i = 0; i < span.count; ++i)
                {
                    ++total;
                    if (!span.entries[i])
                    {
                        ++missing;
                        LOGC(Error, kApiCategory, "init: %s entry point #%zu is NULL (a mod calling it would crash)",
                             span.name, i);
                    }
                }
            }

            if (missing == 0)
                LOGC(Debug, kApiCategory,
                     "init: wired %d entry points across %zu sub-apis (ABI v%u, structSize %u, env %s)",
                     total, subCount, static_cast<unsigned>(api.abiVersion),
                     static_cast<unsigned>(api.structSize),
                     api.environment == CUBE_ENV_CLIENT ? "client" : "server");
            else
                LOGC(Error, kApiCategory, "init: INCOMPLETE - %d/%d entry points wired, %d MISSING (ABI v%u)",
                     total - missing, total, missing, static_cast<unsigned>(api.abiVersion));
            return missing;
        }

    }

    bool fill(CubeApi& api)
    {
        api.abiVersion = CUBE_ABI_VERSION;
        api.structSize = sizeof(CubeApi);
        api.environment = CUBE_ENV_CLIENT; // this loader is the Cube.exe client
        fillSystem(api);
        fillPlayer(api);
        fillCombat(api);
        fillItems(api);
        fillStatus(api);
        fillWorld(api);
        fillEntities(api);
        fillPet(api);
        fillView(api);
        fillSession(api);
        fillCatalog(api);
        fillInput(api);
        fillSelection(api);
        fillPickup(api);
        fillHooks(api);
        return validate(api) == 0;
    }
}
