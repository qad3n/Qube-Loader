#pragma once
// Chat log accessor: read the local chat history and input line (client only, read only). Subscribe
// to a new line with mod.eventListener().onChatMessage(...). A send path is a networked game call and
// is intentionally not exposed.

#include "cube/common.hpp"

namespace cube
{
    // One line from the chat log. Value type over a snapshot.
    class ChatMessage
    {
    public:
        ChatMessage() = default;
        explicit ChatMessage(const CubeChatMessage& data) : m_data(data) {}

        const char* getText() const { return m_data.text; }
        bool hasColor() const { return m_data.hasColor != 0; }
        unsigned char getRed() const { return m_data.colorR; }
        unsigned char getGreen() const { return m_data.colorG; }
        unsigned char getBlue() const { return m_data.colorB; }
        bool empty() const { return m_data.text[0] == '\0'; }
        const CubeChatMessage& raw() const { return m_data; }

    private:
        CubeChatMessage m_data = {};
    };

    // The chat input line + whether chat input is currently open.
    class ChatInput
    {
    public:
        ChatInput() = default;
        explicit ChatInput(const CubeChatInput& data) : m_data(data) {}

        const char* getText() const { return m_data.text; }
        bool isActive() const { return m_data.active != 0; }
        bool valid() const { return m_data.valid != 0; }
        const CubeChatInput& raw() const { return m_data; }

    private:
        CubeChatInput m_data = {};
    };

    // Reads the local chat log. The game keeps the last kMaxMessages lines.
    class Chat
    {
    public:
        explicit Chat(const CubeApi* api) : m_api(api) {}

        static constexpr int kMaxMessages = CUBE_CHAT_MESSAGES_MAX;

        // Fills a caller buffer, newest last. Returns the count written (<= maxCount, <= kMaxMessages).
        int messages(CubeChatMessage* out, int maxCount) const
        {
            return (m_api && out && maxCount > 0)
                       ? static_cast<int>(m_api->chat.messages(m_api, out, maxCount))
                       : 0;
        }

        // Convenience: the newest line (empty if the log is empty or the widget is unavailable).
        ChatMessage newest() const
        {
            CubeChatMessage buf = {};
            return (messages(&buf, 1) == 1) ? ChatMessage(buf) : ChatMessage();
        }

        // The line the player is currently typing + whether chat input is open.
        ChatInput input() const
        {
            CubeChatInput data = {};
            if (m_api)
                m_api->chat.input(m_api, &data);
            return ChatInput(data);
        }

    private:
        const CubeApi* m_api = nullptr;
    };
}
