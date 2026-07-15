#include "api/bridge.h"
#include "game/pickup.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiPickupGetLast(const CubeApi* api, CubeItem* out)
        {
            return bridgeGet<CubeItem>(api, out, &game::pickup::readLast, "pickup.getLast", "none", &CubeItem::address);
        }
    }

    void fillPickup(CubeApi& api)
    {
        api.pickup.getLast = &apiPickupGetLast;
    }
}
