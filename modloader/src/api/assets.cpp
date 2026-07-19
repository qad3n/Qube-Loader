#include "api/bridge.h"
#include "modloader/core/modassets.h"
#include "modloader/core/owner_name.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiAssetsRegister(const CubeApi* api, const char* key, const void* data, int32_t size)
        {
            if (!api || !key)
                return 0;
            if (!capabilityGate(api, CUBE_CAP_ASSETS, "assets.registerAsset"))
                return 0;
            const bool ok = modassets::registerAsset(ownerStem(api), key, data, size);
            LOGC(Debug, kApiCategory, "'%s' assets.registerAsset(%s, %d) -> %s", modName(api), key, size, ok ? "ok" : "fail");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiAssetsUnregister(const CubeApi* api, const char* key)
        {
            if (!api || !key)
                return 0;
            if (!capabilityGate(api, CUBE_CAP_ASSETS, "assets.unregisterAsset"))
                return 0;
            const bool ok = modassets::unregisterAsset(ownerStem(api), key);
            LOGC(Debug, kApiCategory, "'%s' assets.unregisterAsset(%s) -> %s", modName(api), key, ok ? "ok" : "fail");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiAssetsHas(const CubeApi* api, const char* key)
        {
            if (!api || !key)
                return 0;
            const bool has = modassets::hasAsset(ownerStem(api), key);
            LOGC(Trace, kApiCategory, "'%s' assets.hasAsset(%s) -> %d", modName(api), key, has ? 1 : 0);
            return okInt(has);
        }
    }

    void fillAssets(CubeApi& api)
    {
        api.assets.registerAsset = &apiAssetsRegister;
        api.assets.unregisterAsset = &apiAssetsUnregister;
        api.assets.hasAsset = &apiAssetsHas;
    }
}
