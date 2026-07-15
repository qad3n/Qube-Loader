#pragma once
// View tab: camera scalars + view/proj matrices, display/graphics settings, live audio state.

#include "menu/tab.h"

namespace exmod::menu
{

    class ViewTab : public Tab
    {
    public:
        const char* label() const override { return "View"; }
        void draw(const CubeEventArgs& frame) override;

    private:
        static void drawMatrix(const char* label, const float* matrix);
        void drawCamera();
        void drawDisplay();
        void drawAudio();

        int m_soundId = 0; // selected built in sound id for the play button
    };

}
