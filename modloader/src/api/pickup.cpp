#include "api/bridge.h"
#include "loader/game/eventbacking.h"
#include "game/pickup.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        // Same arm-on-first-use rule as selection.getLast: the E/hold-to-pickup capture is installed
        // only once a mod asks for it, and that first call returns "none" because nothing has been
        // captured yet. Held for the session; released by eventbacking::releaseAll at teardown.

        int32_t CUBE_CALL apiPickupGetLast(const CubeApi* api, CubeItem* out)
        {
            eventbacking::acquireOnce(eventbacking::Backing::Pickup);
            return bridgeGet<CubeItem>(api, out, &game::pickup::readLast, "pickup.getLast", "none",
                                       &CubeItem::address);
        }
    }

    void fillPickup(CubeApi& api)
    {
        api.pickup.getLast = &apiPickupGetLast;
    }
}
