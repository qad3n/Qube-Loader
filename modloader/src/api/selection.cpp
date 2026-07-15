#include "api/bridge.h"
#include "game/selection.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiSelectionGetLast(const CubeApi* api, CubeSelection* out)
        {
            return bridgeGet<CubeSelection>(api, out, &game::selection::readLast, "selection.getLast", "none", &CubeSelection::address);
        }
    }

    void fillSelection(CubeApi& api)
    {
        api.selection.getLast = &apiSelectionGetLast;
    }
}
