#include "api/bridge.h"
#include "hooks/input_block.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiInputSetBlocked(const CubeApi* api, int32_t blocked)
        {
            if (!capabilityGate(api, CUBE_CAP_OVERLAY, "input.setBlocked"))
                return 0;
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
