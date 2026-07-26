#include "api/bridge.h"
#include "core/log.h"
#include "game/chat.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiChatMessages(const CubeApi* api, CubeChatMessage* out, int32_t maxCount)
        {
            return bridgeList(api, "chat.messages", maxCount, game::listChatMessages(out, maxCount));
        }

        int32_t CUBE_CALL apiChatInput(const CubeApi* api, CubeChatInput* out)
        {
            return bridgeGet<CubeChatInput>(api, out, &game::readChatInput, "chat.input", "unavailable");
        }
    }

    void fillChat(CubeApi& api)
    {
        api.chat.messages = &apiChatMessages;
        api.chat.input = &apiChatInput;
    }
}
