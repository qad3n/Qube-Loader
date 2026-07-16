#pragma once
// Mod tab: loader/mod info + UI scale, guarded memory read/write, and custom logging.

#include "menu/tab.h"

namespace exmod::menu
{

    constexpr int kLogInputSize = 128;
    constexpr int kConfigInputSize = 128;
    constexpr int kNoteInputSize = 256;
    constexpr int kScopeInputSize = 32;

    class ModTab : public Tab
    {
    public:
        ModTab();

        const char* label() const override { return "Mod"; }
        void draw(const CubeEventArgs& frame) override;

    private:
        struct LogState
        {
            int levelIndex = CUBE_LOG_INFO;
            char message[kLogInputSize] = "";
        };

        void drawInfo(const CubeEventArgs& frame);
        void drawMemory();
        void drawLogging();
        void drawPersist();
        void drawServices(); // inter-mod services + messaging demo (ABI 22)
        void reloadNote(); // pull the storage note for the active scope into m_note

        LogState m_log;
        float m_uiScale = 1.0f;
        char m_greeting[kConfigInputSize] = "";
        bool m_greetOnLoad = true;
        char m_note[kNoteInputSize] = "";  // storage() blob demo (a persistent free-text note)
        char m_scope[kScopeInputSize] = ""; // storage().setScope demo (namespace the note per save)
        int m_pingValue = 21; // services demo: payload sent by the self-ping button
        int m_pingResult = 0; // last self-ping reply
    };

}
