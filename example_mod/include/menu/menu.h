#pragma once
// Public entry point for the debug menu (a main tab sidebar showcasing the modloader API). The Menu
// class and per tab subclasses are internal; the overlay only needs draw(). The tabs are pure views
// over the feature classes (see features/); no game logic lives in the menu.

#include "cube_sdk.h"

namespace exmod::menu
{

    void draw(const CubeEventArgs* frame);

}
