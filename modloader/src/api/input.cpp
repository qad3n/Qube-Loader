#include "api/bridge.h"
#include "hooks/input_block.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiInputSetBlocked(const CubeApi* api, int32_t blocked)
        {
            hooks::input_block::setBlocked(blocked != 0);
            LOGC(Debug, kApiCategory, "'%s' input.setBlocked(%d)", modName(api), blocked);
            return 1;
        }
    }

    void fillInput(CubeApi& api)
    {
        api.input.setBlocked = &apiInputSetBlocked;
    }
}
