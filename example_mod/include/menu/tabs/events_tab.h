#pragma once
// Events tab (OBSERVE side): a pure view over exmod::GameEvents. Live log, per event counters,
// console toggles and the event catalog. All recording/state lives in GameEvents; this tab only
// renders it and flips the per event console toggles.

#include "menu/tab.h"

namespace exmod::menu
{

    class EventsTab : public Tab
    {
    public:
        const char* label() const override { return "Events"; }
        void draw(const CubeEventArgs& frame) override;

    private:
        void drawLog();

        bool m_autoScroll = true; // pure UI preference; the log itself lives in GameEvents
    };

}
