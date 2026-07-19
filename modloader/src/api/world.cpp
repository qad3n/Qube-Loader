#include "api/bridge.h"
#include "core/log.h"
#include "game/world.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiWorldGet(const CubeApi* api, CubeWorld* out)
        {
            return bridgeGet<CubeWorld>(api, out, &game::readWorld, "world.get", "unavailable", &CubeWorld::address);
        }

        int32_t CUBE_CALL apiWorldStructures(const CubeApi* api, CubeStructure* out, int32_t maxCount)
        {
            return bridgeList(api, "world.structures", out, maxCount, game::listStructures(out, maxCount), &CubeStructure::address);
        }

        int32_t CUBE_CALL apiWorldSetTime(const CubeApi* api, int32_t ms)
        {
            if (!capabilityGate(api, CUBE_CAP_WRITES, "world.setTime"))
                return 0;
            writeguard::Scope scope(api);
            const bool ok = game::setWorldTime(ms);
            LOGC(Debug, kApiCategory, "'%s' world.setTime(%d) -> %s", modName(api), ms, ok ? "ok" : "fail");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiWorldSetTile(const CubeApi* api, int32_t field, double value)
        {
            return bridgeSetIndexed(api, "world.setTile", &game::setWorldTile, field, value);
        }

        int32_t CUBE_CALL apiWorldSetSeed(const CubeApi* api, uint32_t seed)
        {
            if (!capabilityGate(api, CUBE_CAP_WRITES, "world.setSeed"))
                return 0;
            writeguard::Scope scope(api);
            const bool ok = game::setWorldSeed(seed);
            LOGC(Debug, kApiCategory, "'%s' world.setSeed(0x%08X) -> %s", modName(api), seed, ok ? "ok" : "fail");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiWorldSetSpawn(const CubeApi* api, float x, float y, float z)
        {
            return bridgeSetVec3(api, "world.setSpawn", &game::setWorldSpawn, x, y, z);
        }

        int32_t CUBE_CALL apiWorldSetStructure(const CubeApi* api, uint32_t address, int32_t field, double value)
        {
            return bridgeSetAddrField(api, "world.setStructure", &game::setStructureField, address, field, value);
        }
    }

    void fillWorld(CubeApi& api)
    {
        api.world.get = &apiWorldGet;
        api.world.structures = &apiWorldStructures;
        api.world.setTime = &apiWorldSetTime;
        api.world.setTile = &apiWorldSetTile;
        api.world.setSeed = &apiWorldSetSeed;
        api.world.setSpawn = &apiWorldSetSpawn;
        api.world.setStructure = &apiWorldSetStructure;
    }
}
