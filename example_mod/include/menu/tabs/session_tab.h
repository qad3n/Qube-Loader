#pragma once
// Session tab: game/network state, open HUD panels, and the last entity the player selected.

#include "menu/tab.h"

namespace exmod::menu
{

    class SessionTab : public Tab
    {
    public:
        const char* label() const override { return "Session"; }
        void draw(const CubeEventArgs& frame) override;

    private:
        void drawState();
        void drawUi();
        void drawSelection();
    };

}
