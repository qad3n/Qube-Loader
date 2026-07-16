#include "api/bridge.h"
#include "modloader/core/services.h"
#include "modloader/core/owner_name.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiServicesRegister(const CubeApi* api, const char* name, uint32_t version, void* impl)
        {
            const bool ok = services::registerService(api, name, version, impl);
            LOGC(Debug, kApiCategory, "'%s' services.register(%s, v%u) -> %s",
                 modName(api), name ? name : "(null)", version, ok ? "ok" : "fail");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiServicesUnregister(const CubeApi* api, const char* name)
        {
            const bool ok = services::unregisterService(api, name);
            LOGC(Debug, kApiCategory, "'%s' services.unregister(%s) -> %s",
                 modName(api), name ? name : "(null)", ok ? "ok" : "fail");
            return okInt(ok);
        }

        void* CUBE_CALL apiServicesQuery(const CubeApi* api, const char* name, uint32_t minVersion, uint32_t* outVersion)
        {
            uint32_t found = 0;
            void* impl = services::query(name, minVersion, &found);
            if (impl && outVersion)
                *outVersion = found;
            LOGC(Debug, kApiCategory, "'%s' services.query(%s, >=v%u) -> %s (v%u)",
                 modName(api), name ? name : "(null)", minVersion, impl ? "hit" : "miss", found);
            return impl;
        }

        uint32_t CUBE_CALL apiServicesOnMessage(const CubeApi* api, CubeMessageFn fn, void* user)
        {
            const uint32_t token = services::addMessageHandler(api, fn, user);
            LOGC(Debug, kApiCategory, "'%s' services.onMessage() -> token %u", modName(api), token);
            return token;
        }

        int32_t CUBE_CALL apiServicesClearMessageHandler(const CubeApi* api, uint32_t token)
        {
            const bool ok = services::removeMessageHandler(api, token);
            LOGC(Debug, kApiCategory, "'%s' services.clearMessageHandler(%u) -> %s", modName(api), token, ok ? "ok" : "fail");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiServicesSendMessage(const CubeApi* api, const char* targetModId, uint32_t msgId, void* payload, uint32_t payloadSize)
        {
            const int32_t result = services::sendMessage(api, targetModId, msgId, payload, payloadSize);
            LOGC(Debug, kApiCategory, "'%s' services.sendMessage(%s, msg %u, %u bytes) -> %d",
                 modName(api), targetModId ? targetModId : "(null)", msgId, payloadSize, result);
            return result;
        }
    }

    void fillServices(CubeApi& api)
    {
        api.services.registerService = &apiServicesRegister;
        api.services.unregisterService = &apiServicesUnregister;
        api.services.query = &apiServicesQuery;
        api.services.onMessage = &apiServicesOnMessage;
        api.services.clearMessageHandler = &apiServicesClearMessageHandler;
        api.services.sendMessage = &apiServicesSendMessage;
    }
}
