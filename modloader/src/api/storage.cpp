#include "api/bridge.h"
#include "modloader/core/modstorage.h"
#include "modloader/core/owner_name.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiStorageSetScope(const CubeApi* api, const char* scope)
        {
            if (!api)
                return 0;
            modstorage::setScope(ownerStem(api), scope ? scope : "");
            LOGC(Debug, kApiCategory, "'%s' storage.setScope(%s)", modName(api), scope ? scope : "(root)");
            return 1;
        }

        int32_t CUBE_CALL apiStorageGet(const CubeApi* api, const char* key, void* out, int32_t size)
        {
            if (!api || !key)
                return 0;
            const int32_t read = modstorage::get(ownerStem(api), key, out, size);
            LOGC(Trace, kApiCategory, "'%s' storage.get(%s, %d) -> %d", modName(api), key, size, read);
            return read;
        }

        int32_t CUBE_CALL apiStoragePut(const CubeApi* api, const char* key, const void* data, int32_t size)
        {
            if (!api || !key)
                return 0;
            const bool ok = modstorage::put(ownerStem(api), key, data, size);
            LOGC(Debug, kApiCategory, "'%s' storage.put(%s, %d) -> %s", modName(api), key, size, ok ? "ok" : "fail");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiStorageRemove(const CubeApi* api, const char* key)
        {
            if (!api || !key)
                return 0;
            const bool ok = modstorage::remove(ownerStem(api), key);
            LOGC(Debug, kApiCategory, "'%s' storage.remove(%s) -> %s", modName(api), key, ok ? "ok" : "absent");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiStorageHas(const CubeApi* api, const char* key)
        {
            if (!api || !key)
                return 0;
            const bool ok = modstorage::has(ownerStem(api), key);
            LOGC(Trace, kApiCategory, "'%s' storage.has(%s) -> %d", modName(api), key, ok);
            return okInt(ok);
        }
    }

    void fillStorage(CubeApi& api)
    {
        api.storage.setScope = &apiStorageSetScope;
        api.storage.get = &apiStorageGet;
        api.storage.put = &apiStoragePut;
        api.storage.remove = &apiStorageRemove;
        api.storage.has = &apiStorageHas;
    }
}
