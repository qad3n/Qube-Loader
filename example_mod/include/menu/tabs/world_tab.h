#pragma once
// World tab: location/seed/spawn, current tile climate, time of day and placed structures, editable.

#include "menu/tab.h"

#include "cube_mod.hpp"

namespace exmod::menu
{

    class WorldTab : public Tab
    {
    public:
        const char* label() const override { return "World"; }
        void draw(const CubeEventArgs& frame) override;

    private:
        void drawLocation(cube::World& world);
        void drawClimate(cube::World& world);
        void drawTime(cube::World& world);
        void drawStructures();
    };

}
