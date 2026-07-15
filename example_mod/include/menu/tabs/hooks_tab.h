#pragma once
// Hooks tab (INTERCEPT side): a pure view over exmod::GameHooks. Built in impact/crit/max HP toggles
// + a raw address hook installer. The interception handlers, toggles and counters live in GameHooks;
// this tab only binds widgets to its settings and holds the transient raw hook input fields.

#include "menu/tab.h"

namespace exmod::menu
{

    class HooksTab : public Tab
    {
    public:
        const char* label() const override { return "Hooks"; }
        void draw(const CubeEventArgs& frame) override;

    private:
        void drawBuiltin();
        void drawRaw();
    };

}
