#pragma once
// Combat tab: stored stats, telemetry, buffs and live ability cooldowns. Pure view: the rolling
// health history plot is owned + sampled by exmod::HealthHistory; this tab only plots its data.

#include "menu/tab.h"

#include "cube_mod.hpp"

namespace exmod::menu
{

    class CombatTab : public Tab
    {
    public:
        const char* label() const override { return "Combat"; }
        void draw(const CubeEventArgs& frame) override;

    private:
        void drawStats(cube::Combat& combat, cube::Hero& hero);
        void drawTelemetry(cube::Combat& combat, cube::Hero& hero);
        void drawBuffs();
        void drawAbilities();

        // Transient inputs for the set cooldown by id control (pure widget state).
        int m_cdAbilityId = 0;
        int m_cdMs = 0;
    };

}
