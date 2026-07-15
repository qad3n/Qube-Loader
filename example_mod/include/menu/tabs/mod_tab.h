#pragma once
// Mod tab: loader/mod info + UI scale, guarded memory read/write, and custom logging.

#include "menu/tab.h"

namespace exmod::menu
{

    constexpr int kLogInputSize = 128;

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

        LogState m_log;
        float m_uiScale = 1.0f;
    };

}
