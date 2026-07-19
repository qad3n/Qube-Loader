#include "menu/tabs/entities_tab.h"
#include "mod_context.h"

#include "imgui.h"

#include <vector>

namespace exmod::menu
{
    namespace
    {
        // Entity type is the loader's relation (from the +0x60 kind byte, kind 1 split into hostile
        // monsters vs peaceful animals). Raw kind shown per row; kind 3 has no name in the binary.
        const ImVec4 kEnemyColor = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
        const ImVec4 kAnimalColor = ImVec4(0.8f, 0.72f, 0.45f, 1.0f);
        const ImVec4 kPetColor = ImVec4(0.4f, 0.9f, 0.4f, 1.0f);
        const ImVec4 kNpcColor = ImVec4(0.4f, 0.8f, 1.0f, 1.0f);
        const ImVec4 kPlayerColor = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
        const ImVec4 kUnknownColor = ImVec4(0.8f, 0.6f, 1.0f, 1.0f);
        const ImVec4 kSelfColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        const ImVec4& relationColor(cube::Relation r)
        {
            switch (r)
            {
                case cube::Relation::Hostile:
                    return kEnemyColor;
                case cube::Relation::Neutral:
                    return kAnimalColor;
                case cube::Relation::OwnPet:
                    return kPetColor;
                case cube::Relation::Npc:
                    return kNpcColor;
                case cube::Relation::Player:
                    return kPlayerColor;
                case cube::Relation::Self:
                    return kSelfColor;
                default:
                    return kUnknownColor;
            }
        }

        const ImVec4& entityColor(const cube::Entity& e)
        {
            return relationColor(e.getRelation());
        }

        const char* entityLabel(const cube::Entity& e)
        {
            return cube::relationName(e.getRelation());
        }

        void legendEntry(cube::Relation r, bool last)
        {
            ImGui::TextColored(relationColor(r), "%s", cube::relationName(r));
            if (!last)
                ImGui::SameLine();
        }

        void drawRelationLegend()
        {
            legendEntry(cube::Relation::Hostile, false);
            legendEntry(cube::Relation::Neutral, false);
            legendEntry(cube::Relation::OwnPet, false);
            legendEntry(cube::Relation::Npc, false);
            legendEntry(cube::Relation::Player, false);
            legendEntry(cube::Relation::Unknown, true);
        }

    }

    template <typename Creature>
    void EntitiesTab::drawTransformEditors(const Creature& creature, char* nameBuf, size_t nameSize, cube::Hero& hero, const char* teleportLabel)
    {
        float facing = creature.getFacing();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::SliderAngle("facing", &facing))
            creature.setFacing(facing);
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::InputText("name", nameBuf, nameSize);
        ImGui::SameLine();
        if (ImGui::SmallButton("Set##name"))
            creature.setName(nameBuf);
        const cube::Vec3 pos = creature.getPosition();
        const cube::Vec3 vel = creature.getVelocity();
        float position[3] = {pos.x, pos.y, pos.z};
        float velocity[3] = {vel.x, vel.y, vel.z};
        ImGui::SetNextItemWidth(sc(kTeleportInputWidth));
        if (ImGui::DragFloat3("position", position, kStatDragSpeed))
            creature.teleport(position[0], position[1], position[2]);
        ImGui::SetNextItemWidth(sc(kTeleportInputWidth));
        if (ImGui::DragFloat3("velocity", velocity, kFineDragSpeed))
            creature.setVelocity(velocity[0], velocity[1], velocity[2]);
        if (ImGui::SmallButton(teleportLabel) && hero.valid())
            hero.teleport(creature.getPosition());
    }

    // Read only fields plus live editors for one creature. Callers must push a unique ID scope.
    void EntitiesTab::drawEntityDetail(const cube::Entity& entity, cube::Hero& hero)
    {
        if (beginTable("ent_detail"))
        {
            row("Address", "0x%08X", entity.raw().address);
            row("Name", "%s", entity.getName()[0] ? entity.getName() : "(unnamed)");
            row("Type", "%s (kind %d)", entityLabel(entity), entity.getCategory());
            row("Hostile", "%s", yesNo(entity.isHostile()));
            row("State", "%s (from health)", cube::entityStateName(entity.getState()));
            row("Distance", "%.1f m", entity.getDistance());
            row("Species", "%s (#%d)",
                cube::catalog::nameOr(g_api, CUBE_CATALOG_SPECIES, entity.getType(), "unknown"), entity.getType());
            row("Class", "%s",
                cube::catalog::nameOr(g_api, CUBE_CATALOG_CLASS, entity.getCombatClass(), "none"));
            row("Boss / rank", "%s / %d stars", yesNo(entity.isBoss()), entity.getRank());
            row("Elite", "%s (boss or 3+ stars)", yesNo(entity.isElite()));
            row("Effective power", "%d (vs your level)", entity.getEffectivePower());
            row("Stagger", "hitStun %d%s", entity.getHitStun(),
                entity.isKnockedDown() ? " (knocked down)" : "");
            if (entity.getOwnerAddress() != 0)
                row("Owner addr", "0x%08X", entity.getOwnerAddress());
            row("Weapon", "%s", entity.getWeapon().getName());
            row("Tameable", "%s (feed a Food item to tame)", yesNo(entity.isTameable()));
            ImGui::EndTable();
        }
        // Any creature's inventory (a shopkeeper's wares, a chest's loot) read via entity.stock().
        const std::vector<cube::Item> stock = entity.stock();
        if (!stock.empty())
        {
            ImGui::SeparatorText("stock / inventory");
            for (size_t i = 0; i < stock.size(); ++i)
            {
                const cube::Item& ware = stock[i];
                ImGui::BulletText("%s  x%d  (%d coins)", ware.getName(), ware.getStack(), ware.getValue());
            }
        }
        const std::vector<cube::Buff> effects = entity.buffs();
        if (!effects.empty())
        {
            ImGui::SeparatorText("status effects");
            for (size_t i = 0; i < effects.size(); ++i)
            {
                const cube::Buff& effect = effects[i];
                const char* effectName = cube::catalog::nameOr(g_api, CUBE_CATALOG_BUFF_TYPE, effect.getType(), "effect");
                ImGui::BulletText("%s (#%d)  x%.2f  %d ms", effectName, effect.getType(),
                                  effect.getMagnitude(), effect.getRemainingMs());
            }
        }
        if ((entity.getHitStun() > 0 || entity.isKnockedDown()) && ImGui::SmallButton("Break free##entstun"))
            entity.clearStun();
        int value = 0;
        if (idEditor("species", CUBE_CATALOG_SPECIES, entity.getType(), value))
            entity.setType(value);
        if (idEditor("category", CUBE_CATALOG_ENTITY_CATEGORY, entity.getCategory(), value))
            entity.setCategory(value);
        float health = entity.getHealth();
        if (dragFloat("health", health, kStatDragSpeed, kHealthMin, kHealthMax, "%.0f"))
            entity.setHealth(health);
        int level = entity.getLevel();
        if (dragInt("level", level, kIntDragSpeed, kSmallCountMin, kLevelMax))
            entity.setLevel(level);
        int rank = entity.getRank();
        if (dragInt("rank", rank, kIntDragSpeed, kSmallCountMin, kSmallCountMax))
            entity.setRank(rank);
        drawTransformEditors(entity, m_entityName, sizeof(m_entityName), hero, "Teleport me here");
    }

    void EntitiesTab::drawNearby(cube::Hero& hero)
    {
        // Loader returns entities nearest first, so index 0 is the closest creature.
        std::vector<cube::Entity> nearby = cube::entitiesOf(g_api);
        const int total = static_cast<int>(nearby.size());
        ImGui::Text("%d nearby (expand any row for full detail)", total);
        drawRelationLegend();
        // Show EVERY nearby creature the loader returned; a scroll region keeps the
        // full list usable no matter how many are loaded.
        ImGui::BeginChild("ent_list", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        for (int i = 0; i < total; ++i)
        {
            const cube::Entity& entity = nearby[static_cast<size_t>(i)];
            // Key the row by live creature address, never the loop index: the entity set reorders
            // every frame, so an index keyed node's open state + actions would hit the wrong creature.
            ImGui::PushID(static_cast<int>(entity.raw().address));
            ImGui::PushStyleColor(ImGuiCol_Text, entityColor(entity));
            const bool open = ImGui::TreeNode("row", "%s  L%d  %.0fm  %s (kind %d)%s",
                                              entity.getName()[0] ? entity.getName() : "(unnamed)", entity.getLevel(),
                                              entity.getDistance(), entityLabel(entity), entity.getCategory(),
                                              entity.isBoss() ? " BOSS" : "");
            ImGui::PopStyleColor();
            if (open)
            {
                drawEntityDetail(entity, hero);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
    }

    void EntitiesTab::drawPet(cube::Hero& hero)
    {
        cube::Pet pet(g_api);
        if (!pet.valid())
        {
            ImGui::TextDisabled("no active pet");
            return;
        }
        if (beginTable("en_pet"))
        {
            row("Address", "0x%08X", pet.raw().address);
            row("XP", "%u", pet.getXp());
            row("State", "%s", cube::entityStateName(pet.getState()));
            ImGui::EndTable();
        }
        int value = 0;
        if (idEditor("species", CUBE_CATALOG_SPECIES, pet.getType(), value))
            pet.setType(value);
        float health = pet.getHealth();
        if (dragFloat("health", health, kStatDragSpeed, kHealthMin, kHealthMax, "%.0f"))
            pet.setHealth(health);
        int level = pet.getLevel();
        if (dragInt("level", level, kIntDragSpeed, kSmallCountMin, kLevelMax))
            pet.setLevel(level);
        drawTransformEditors(pet, m_petName, sizeof(m_petName), hero, "Teleport me to pet");
    }

    void EntitiesTab::draw(const CubeEventArgs&)
    {
        cube::Hero hero(g_api);
        if (!ImGui::BeginTabBar("##enttabs"))
            return;
        if (ImGui::BeginTabItem("Nearby"))
        {
            drawNearby(hero);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Target"))
        {
            cube::Entity target;
            if (!cube::targetOf(g_api, target))
                ImGui::TextDisabled("no target");
            else
            {
                ImGui::PushID("tgt");
                drawEntityDetail(target, hero);
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Aim"))
        {
            ImGui::TextDisabled("crosshair hover target (what you are looking at)");
            cube::Entity aim;
            if (!cube::aimTargetOf(g_api, aim))
                ImGui::TextDisabled("not aiming at a creature");
            else
            {
                ImGui::PushID("aim");
                drawEntityDetail(aim, hero);
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Pet"))
        {
            drawPet(hero);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

}
