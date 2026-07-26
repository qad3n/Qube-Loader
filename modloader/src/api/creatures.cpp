#include "api/bridge.h"
#include "core/log.h"
#include "game/creatures.h"
#include "game/status.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {

        int32_t CUBE_CALL apiCreaturesList(const CubeApi* api, CubeCreature* out, int32_t maxCount)
        {
            return bridgeList(api, "creatures.list", out, maxCount, game::listCreatures(out, maxCount), &CubeCreature::address);
        }

        int32_t CUBE_CALL apiCreaturesTarget(const CubeApi* api, CubeCreature* out)
        {
            return bridgeGet<CubeCreature>(api, out, &game::targetEntity, "creatures.target", "none", &CubeCreature::address);
        }

        int32_t CUBE_CALL apiCreaturesAimTarget(const CubeApi* api, CubeCreature* out)
        {
            return bridgeGet<CubeCreature>(api, out, &game::aimTargetEntity, "creatures.aimTarget", "none", &CubeCreature::address);
        }

        int32_t CUBE_CALL apiCreaturesEffects(const CubeApi* api, uint32_t address, CubeBuff* out, int32_t maxCount)
        {
            return bridgeList(api, "creatures.effects", out, maxCount, game::listBuffsOfAddress(address, out, maxCount), &CubeBuff::address);
        }

        int32_t CUBE_CALL apiCreaturesSetStat(const CubeApi* api, uint32_t address, int32_t stat, double value)
        {
            return bridgeSetAddrField(api, "creatures.setStat", &game::setEntityStat, address, stat, value);
        }

        int32_t CUBE_CALL apiCreaturesSetName(const CubeApi* api, uint32_t address, const char* name)
        {
            return bridgeSetAddrName(api, "creatures.setName", &game::setEntityName, address, name);
        }

        int32_t CUBE_CALL apiCreaturesTeleport(const CubeApi* api, uint32_t address, float x, float y, float z)
        {
            return bridgeSetAddrVec3(api, "creatures.teleport", &game::teleportEntity, address, x, y, z);
        }

        int32_t CUBE_CALL apiCreaturesIsTameable(const CubeApi* api, uint32_t address)
        {
            const bool ok = game::isCreatureTameable(address);
            LOGC(Trace, kApiCategory, "'%s' creatures.isTameable(0x%08X) -> %s", modName(api), address, ok ? "yes" : "no");
            return okInt(ok);
        }

    }

    void fillCreatures(CubeApi& api)
    {
        api.creatures.list = &apiCreaturesList;
        api.creatures.target = &apiCreaturesTarget;
        api.creatures.aimTarget = &apiCreaturesAimTarget;
        api.creatures.effects = &apiCreaturesEffects;
        api.creatures.setStat = &apiCreaturesSetStat;
        api.creatures.setName = &apiCreaturesSetName;
        api.creatures.teleport = &apiCreaturesTeleport;
        api.creatures.isTameable = &apiCreaturesIsTameable;
    }
}