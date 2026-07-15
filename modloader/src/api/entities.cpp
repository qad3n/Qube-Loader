#include "api/bridge.h"
#include "core/log.h"
#include "game/entities.h"
#include "game/status.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {

        int32_t CUBE_CALL apiEntitiesList(const CubeApi* api, CubeEntity* out, int32_t maxCount)
        {
            return bridgeList(api, "entities.list", out, maxCount, game::listEntities(out, maxCount), &CubeEntity::address);
        }

        int32_t CUBE_CALL apiEntitiesTarget(const CubeApi* api, CubeEntity* out)
        {
            return bridgeGet<CubeEntity>(api, out, &game::targetEntity, "entities.target", "none", &CubeEntity::address);
        }

        int32_t CUBE_CALL apiEntitiesAimTarget(const CubeApi* api, CubeEntity* out)
        {
            return bridgeGet<CubeEntity>(api, out, &game::aimTargetEntity, "entities.aimTarget", "none", &CubeEntity::address);
        }

        int32_t CUBE_CALL apiEntitiesEffects(const CubeApi* api, uint32_t address, CubeBuff* out, int32_t maxCount)
        {
            return bridgeList(api, "entities.effects", out, maxCount, game::listBuffsOfAddress(address, out, maxCount), &CubeBuff::address);
        }

        int32_t CUBE_CALL apiEntitiesSetStat(const CubeApi* api, uint32_t address, int32_t stat, double value)
        {
            return bridgeSetAddrField(api, "entities.setStat", &game::setEntityStat, address, stat, value);
        }

        int32_t CUBE_CALL apiEntitiesSetName(const CubeApi* api, uint32_t address, const char* name)
        {
            return bridgeSetAddrName(api, "entities.setName", &game::setEntityName, address, name);
        }

        int32_t CUBE_CALL apiEntitiesTeleport(const CubeApi* api, uint32_t address, float x, float y, float z)
        {
            return bridgeSetAddrVec3(api, "entities.teleport", &game::teleportEntity, address, x, y, z);
        }

        int32_t CUBE_CALL apiEntitiesIsTameable(const CubeApi* api, uint32_t address)
        {
            const bool ok = game::isCreatureTameable(address);
            LOGC(Trace, kApiCategory, "'%s' entities.isTameable(0x%08X) -> %s", modName(api), address, ok ? "yes" : "no");
            return okInt(ok);
        }

    }

    void fillEntities(CubeApi& api)
    {
        api.entities.list = &apiEntitiesList;
        api.entities.target = &apiEntitiesTarget;
        api.entities.aimTarget = &apiEntitiesAimTarget;
        api.entities.effects = &apiEntitiesEffects;
        api.entities.setStat = &apiEntitiesSetStat;
        api.entities.setName = &apiEntitiesSetName;
        api.entities.teleport = &apiEntitiesTeleport;
        api.entities.isTameable = &apiEntitiesIsTameable;
    }
}