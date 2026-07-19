#include "api/bridge.h"
#include "core/log.h"
#include "game/player.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiPlayerGet(const CubeApi* api, CubePlayer* out)
        {
            return bridgeGet<CubePlayer>(api, out, &game::readPlayer, "player.get", "unavailable", &CubePlayer::address);
        }

        int32_t CUBE_CALL apiPlayerAddXp(const CubeApi* api, int32_t amount)
        {
            if (!capabilityGate(api, CUBE_CAP_WRITES, "player.addXp"))
                return 0;
            writeguard::Scope scope(api);
            const bool ok = game::addPlayerXp(amount);
            LOGC(Debug, kApiCategory, "'%s' player.addXp(%d) -> %s", modName(api), amount, ok ? "ok" : "fail");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiPlayerTeleport(const CubeApi* api, float x, float y, float z)
        {
            return bridgeSetVec3(api, "player.teleport", &game::teleportPlayer, x, y, z);
        }

        int32_t CUBE_CALL apiPlayerSetStat(const CubeApi* api, int32_t stat, double value)
        {
            if (!capabilityGate(api, CUBE_CAP_WRITES, "player.setStat"))
                return 0;
            writeguard::Scope scope(api);
            const bool ok = game::setPlayerStat(stat, value);
            LOGC(Debug, kApiCategory, "'%s' player.setStat(%d, %.3f) -> %s", modName(api), stat, value, ok ? "ok" : "fail");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiPlayerSetName(const CubeApi* api, const char* name)
        {
            return bridgeSetName(api, "player.setName", &game::setPlayerName, name);
        }
    }

    void fillPlayer(CubeApi& api)
    {
        api.player.get = &apiPlayerGet;
        api.player.addXp = &apiPlayerAddXp;
        api.player.teleport = &apiPlayerTeleport;
        api.player.setStat = &apiPlayerSetStat;
        api.player.setName = &apiPlayerSetName;
    }
}
