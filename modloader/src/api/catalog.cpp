#include "api/bridge.h"
#include "core/log.h"
#include "game/catalog.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {

        int32_t CUBE_CALL apiCatalogList(const CubeApi* api, int32_t catalog, CubeNamedValue* out, int32_t maxCount)
        {
            const int32_t count = game::catalogList(catalog, out, maxCount);
            LOGC(Trace, kApiCategory, "'%s' catalog.list(%d, max %d) -> %d", modName(api), catalog, maxCount, count);
            return count;
        }

        const char* CUBE_CALL apiCatalogName(const CubeApi* api, int32_t catalog, int32_t id)
        {
            const char* name = game::catalogName(catalog, id);
            LOGC(Trace, kApiCategory, "'%s' catalog.name(%d, %d) -> %s", modName(api), catalog, id, name ? name : "(null)");
            return name;
        }
    }

    void fillCatalog(CubeApi& api)
    {
        api.catalog.list = &apiCatalogList;
        api.catalog.name = &apiCatalogName;
    }
}