#pragma once
// Hero tab: the local player's vitals/progression/movement/identity, each an editor. Pure view: the
// continuous cheats (god mode, infinite mana/stamina) are owned + applied by exmod::Cheats; this tab
// only binds widgets to their settings.

#include "menu/tab.h"

#include "cube_mod.hpp"

namespace exmod::menu
{

    constexpr int kQuickXpSmall = 100;
    constexpr int kQuickCoins = 1000;

    class HeroTab : public Tab
    {
    public:
        const char* label() const override { return "Hero"; }
        void draw(const CubeEventArgs& frame) override;

    private:
        // Persistent widget inputs (typed, then applied on a button press).
        struct Inputs
        {
            int level = 1;
            int xpAmount = kQuickXpSmall;
            int coins = kQuickCoins;
            float teleport[3] = {0.0f, 0.0f, 0.0f};
        };

        void drawVitals(cube::Hero& hero);
        void drawProgress(cube::Hero& hero);
        void drawMovement(cube::Hero& hero);
        void drawIdentity(cube::Hero& hero);

        Inputs m_inputs;
        char m_name[CUBE_PLAYER_NAME_MAX] = "Hero";
    };

}
