#pragma once
// Public entry point for the debug menu (a main tab sidebar showcasing the modloader API). The Menu
// class and per tab subclasses are internal; the loader-owned overlay only needs draw(). The tabs are
// pure views over the feature classes (see features/); no game logic lives in the menu.

namespace exmod::menu
{

    // Called from the mod's mod.menu().onDraw callback each visible frame (the loader has already bound
    // its shared ImGui context + run NewFrame). Draws the whole window; takes no args - per-frame data
    // it needs (frame count, framerate) comes straight from ImGui.
    void draw();

}
