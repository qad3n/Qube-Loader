#include "api/bridge.h"
#include "game/companion.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiCompanionGet(const CubeApi* api, CubeCompanion* out)
        {
            return bridgeGet<CubeCompanion>(api, out, &game::readCompanion, "pet.get", "none", &CubeCompanion::address);
        }
    }

    void fillCompanion(CubeApi& api)
    {
        api.companion.get = &apiCompanionGet;
    }
}
