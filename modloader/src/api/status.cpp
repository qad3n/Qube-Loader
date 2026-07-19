#include "api/bridge.h"
#include "game/status.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {

        int32_t CUBE_CALL apiStatusEffects(const CubeApi* api, CubeBuff* out, int32_t maxCount)
        {
            return bridgeList(api, "status.effects", out, maxCount, game::listBuffs(out, maxCount), &CubeBuff::address);
        }

        int32_t CUBE_CALL apiStatusSetField(const CubeApi* api, uint32_t address, int32_t field, double value)
        {
            return bridgeSetAddrField(api, "status.setField", &game::setBuffField, address, field, value);
        }

        int32_t CUBE_CALL apiStatusStun(const CubeApi* api, uint32_t address, CubeStun* out)
        {
            if (!out)
                return 0;
            CubeStun value = {};
            value.structSize = sizeof(CubeStun);
            const bool ok = game::readStun(address, value);
            LOGC(Trace, kApiCategory, "'%s' status.stun(0x%08X) -> %s", modName(api), address, ok ? "ok" : "unavailable");
            if (!ok)
                return 0;
            *out = value;
            return 1;
        }

        int32_t CUBE_CALL apiStatusClearStun(const CubeApi* api, uint32_t address)
        {
            if (!capabilityGate(api, CUBE_CAP_WRITES, "status.clearStun"))
                return 0;
            writeguard::Scope scope(api);
            const bool ok = game::clearStun(address);
            LOGC(Debug, kApiCategory, "'%s' status.clearStun(0x%08X) -> %s", modName(api), address, ok ? "ok" : "fail");
            return okInt(ok);
        }

    }

    void fillStatus(CubeApi& api)
    {
        api.status.effects = &apiStatusEffects;
        api.status.setField = &apiStatusSetField;
        api.status.stun = &apiStatusStun;
        api.status.clearStun = &apiStatusClearStun;
    }

}
