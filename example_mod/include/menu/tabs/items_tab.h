#pragma once
// Items tab: equipment slots, inventory bag and skill ranks, each row an editor. Item edits are
// validated/clamped in the loader so an edit cannot build an unrenderable ("?") item.

#include "menu/tab.h"

#include "cube_mod.hpp"

namespace exmod::menu
{

    class ItemsTab : public Tab
    {
    public:
        const char* label() const override { return "Items"; }
        void draw(const CubeEventArgs& frame) override;

    private:
        void drawItemNode(const cube::Item& item, const char* strId);
        void drawEquipment();
        void drawBag();
        void drawSkills();
    };

}
