#include "api/bridge.h"
#include "core/log.h"
#include "game/appearance.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        bool readLocalAppearance(CubeAppearance& out)
        {
            return game::readAppearance(0, out);
        }

        int32_t CUBE_CALL apiAppearanceGet(const CubeApi* api, CubeAppearance* out)
        {
            return bridgeGet<CubeAppearance>(api, out, &readLocalAppearance, "appearance.get", "unavailable",
                                             &CubeAppearance::address);
        }

        int32_t CUBE_CALL apiAppearanceOf(const CubeApi* api, uint32_t creatureAddress, CubeAppearance* out)
        {
            if (!out)
                return 0;
            CubeAppearance value = {};
            value.structSize = sizeof(CubeAppearance);
            const bool ok = game::readAppearance(creatureAddress, value);
            LOGC(Trace, kApiCategory, "'%s' appearance.of(0x%08X) -> %s (0x%08X)", modName(api),
                 creatureAddress, ok ? "ok" : "unavailable", value.address);
            if (!ok)
                return 0;
            *out = value;
            return 1;
        }
    }

    void fillAppearance(CubeApi& api)
    {
        api.appearance.get = &apiAppearanceGet;
        api.appearance.of = &apiAppearanceOf;
    }
}
