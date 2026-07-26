#pragma once
// Reads the local chat log widget (ui/ChatWidget). Client only, read only. Reachability, the intrusive
// list walk and the UTF-16 to ASCII downconvert all live here, so the api/wrapper layers never see an
// address. The game keeps only the last off::kChatMaxMessages lines.

#include "cube_sdk.h"
#include <cstdint>

namespace game
{
    // Fills out[0..count) with the chat log, newest last. Returns the count written (<= maxCount,
    // <= off::kChatMaxMessages). 0 if the widget is unavailable or the log is empty.
    int32_t listChatMessages(CubeChatMessage* out, int32_t maxCount);

    // Reads the input line + active flag into out. Returns true if the widget resolved.
    bool readChatInput(CubeChatInput& out);

    // Cheap per frame change probe for CUBE_EVENT_CHAT_MESSAGE. Sets countOut to the live message count
    // and sigOut to a hash of the newest line (0 when empty). Returns true if the widget resolved.
    bool chatProbe(int32_t& countOut, uint32_t& sigOut);
}
