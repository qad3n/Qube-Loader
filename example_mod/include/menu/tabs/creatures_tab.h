#pragma once
// Entities tab: nearby creature list (nearest first, coloured by relation), committed target, aim
// target and pet. Every row is a full editor for that creature (health/level/type/facing/...).

#include "menu/tab.h"

#include "cube_mod.hpp"

namespace exmod::menu
{

    class CreaturesTab : public Tab
    {
    public:
        const char* label() const override { return "Entities"; }
        void draw(const CubeEventArgs& frame) override;

    private:
        void drawCreatureDetail(const cube::Creature& creature, cube::Player& player);
        void drawNearby(cube::Player& player);
        void drawCompanion(cube::Player& player);
        // Facing/name/position/velocity editors shared by the creature detail and pet views (both are
        // Creatures with the same setters). teleportLabel names the "warp me to it" button.
        template <typename CreatureT>
        void drawTransformEditors(const CreatureT& creature, char* nameBuf, size_t nameSize,
                                  cube::Player& player, const char* teleportLabel);

        // Separate name edit buffers so creature / pet do not stomp each other.
        char m_entityName[CUBE_PLAYER_NAME_MAX] = "";
        char m_petName[CUBE_PLAYER_NAME_MAX] = "";
    };

}
