#include "api/bridge.h"
#include "game/session.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {

        int32_t CUBE_CALL apiSessionGet(const CubeApi* api, CubeSession* out)
        {
            return bridgeGet<CubeSession>(api, out, &game::readSession, "session.get", "unavailable");
        }

        int32_t CUBE_CALL apiSessionSetField(const CubeApi* api, int32_t field, int32_t value)
        {
            return bridgeSetPair(api, "session.setField", &game::setSessionField, field, value);
        }

        int32_t CUBE_CALL apiUiGet(const CubeApi* api, CubeUi* out)
        {
            if (!capabilityGate(api, CUBE_CAP_OVERLAY, "ui.get"))
                return 0;
            return bridgeGet<CubeUi>(api, out, &game::readUi, "ui.get", "unavailable");
        }

        int32_t CUBE_CALL apiUiSetField(const CubeApi* api, int32_t field, int32_t open)
        {
            return bridgeSetPair(api, "ui.setField", &game::setUiField, field, open);
        }

    }

    void fillSession(CubeApi& api)
    {
        api.session.get = &apiSessionGet;
        api.session.setField = &apiSessionSetField;
        api.ui.get = &apiUiGet;
        api.ui.setField = &apiUiSetField;
    }
}
