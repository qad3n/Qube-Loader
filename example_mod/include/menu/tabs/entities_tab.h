#pragma once
// Entities tab: nearby creature list (nearest first, coloured by relation), committed target, aim
// target and pet. Every row is a full editor for that creature (health/level/type/facing/...).

#include "menu/tab.h"

#include "cube_mod.hpp"

namespace exmod::menu
{

    class EntitiesTab : public Tab
    {
    public:
        const char* label() const override { return "Entities"; }
        void draw(const CubeEventArgs& frame) override;

    private:
        void drawEntityDetail(const cube::Entity& entity, cube::Hero& hero);
        void drawNearby(cube::Hero& hero);
        void drawPet(cube::Hero& hero);
        // Facing/name/position/velocity editors shared by the entity-detail and pet views (both are
        // Creatures with the same setters). teleportLabel names the "warp me to it" button.
        template <typename Creature>
        void drawTransformEditors(const Creature& creature, char* nameBuf, size_t nameSize, cube::Hero& hero, const char* teleportLabel);

        // Separate name edit buffers so entity / pet do not stomp each other.
        char m_entityName[CUBE_PLAYER_NAME_MAX] = "";
        char m_petName[CUBE_PLAYER_NAME_MAX] = "";
    };

}
