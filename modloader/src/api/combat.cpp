#include "api/bridge.h"
#include "game/combat.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {

        int32_t CUBE_CALL apiCombatGet(const CubeApi* api, CubeCombat* out)
        {
            return bridgeGet<CubeCombat>(api, out, &game::readCombat, "combat.get", "unavailable", &CubeCombat::address);
        }

    }

    void fillCombat(CubeApi& api)
    {
        api.combat.get = &apiCombatGet;
    }
}