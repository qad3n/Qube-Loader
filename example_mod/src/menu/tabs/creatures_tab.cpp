#include "menu/tabs/creatures_tab.h"
#include "mod_context.h"

#include "imgui.h"

#include <vector>

namespace exmod::menu
{
    namespace
    {
        // Creature type is the loader's relation (from the +0x60 kind byte, kind 1 split into hostile
        // monsters vs peaceful animals). Raw kind shown per row; kind 3 has no name in the binary.
        const ImVec4 kEnemyColor = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
        const ImVec4 kAnimalColor = ImVec4(0.8f, 0.72f, 0.45f, 1.0f);
        const ImVec4 kCompanionColor = ImVec4(0.4f, 0.9f, 0.4f, 1.0f);
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
                case cube::Relation::OwnCompanion:
                    return kCompanionColor;
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

        const ImVec4& creatureColor(const cube::Creature& e)
        {
            return relationColor(e.getRelation());
        }

        const char* creatureLabel(const cube::Creature& e)
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
            legendEntry(cube::Relation::OwnCompanion, false);
            legendEntry(cube::Relation::Npc, false);
            legendEntry(cube::Relation::Player, false);
            legendEntry(cube::Relation::Unknown, true);
        }

    }

    template <typename CreatureT>
    void CreaturesTab::drawTransformEditors(const CreatureT& creature, char* nameBuf, size_t nameSize, cube::Player& player, const char* teleportLabel)
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
        if (ImGui::SmallButton(teleportLabel) && player.valid())
            player.teleport(creature.getPosition());
    }

    // Read only fields plus live editors for one creature. Callers must push a unique ID scope.
    void CreaturesTab::drawCreatureDetail(const cube::Creature& creature, cube::Player& player)
    {
        if (beginTable("ent_detail"))
        {
            row("Address", "0x%08X", creature.raw().address);
            row("Name", "%s", creature.getName()[0] ? creature.getName() : "(unnamed)");
            row("Type", "%s (kind %d)", creatureLabel(creature), creature.getCategory());
            row("Hostile", "%s", yesNo(creature.isHostile()));
            row("State", "%s (from health)", cube::creatureStateName(creature.getState()));
            row("Distance", "%.1f m", creature.getDistance());
            row("Species", "%s (#%d)",
                cube::catalog::nameOr(g_api, CUBE_CATALOG_SPECIES, creature.getType(), "unknown"), creature.getType());
            row("Class", "%s",
                cube::catalog::nameOr(g_api, CUBE_CATALOG_CLASS, creature.getCombatClass(), "none"));
            row("Boss / rank", "%s / %d stars", yesNo(creature.isBoss()), creature.getRank());
            row("Elite", "%s (boss or 3+ stars)", yesNo(creature.isElite()));
            row("Effective power", "%d (vs your level)", creature.getEffectivePower());
            row("Stagger", "hitStun %d%s", creature.getHitStun(),
                creature.isKnockedDown() ? " (knocked down)" : "");
            if (creature.getOwnerAddress() != 0)
                row("Owner addr", "0x%08X", creature.getOwnerAddress());
            // getWeapon() resolves the main-hand slot (equip index 6 / Creature+0xaa8), the weapon the
            // game's combat code actually swings. The off-hand (index 5) is a separate slot.
            const cube::Item weapon = creature.getWeapon();
            row("Weapon (main hand)", "%s", weapon.present() ? weapon.getName() : "(none)");
            row("Tameable", "%s (feed a Food item to tame)", yesNo(creature.isTameable()));
            ImGui::EndTable();
        }
        // Any creature's inventory (a shopkeeper's wares, a chest's loot) read via creature.stock().
        const std::vector<cube::Item> stock = creature.stock();
        if (!stock.empty())
        {
            ImGui::SeparatorText("stock / inventory");
            for (size_t i = 0; i < stock.size(); ++i)
            {
                const cube::Item& ware = stock[i];
                ImGui::BulletText("%s  x%d  (%d coins)", ware.getName(), ware.getStack(), ware.getValue());
            }
        }
        const std::vector<cube::Buff> effects = creature.buffs();
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
        if ((creature.getHitStun() > 0 || creature.isKnockedDown()) && ImGui::SmallButton("Break free##entstun"))
            creature.clearStun();
        int value = 0;
        if (idEditor("species", CUBE_CATALOG_SPECIES, creature.getType(), value))
            creature.setType(value);
        if (idEditor("category", CUBE_CATALOG_CREATURE_CATEGORY, creature.getCategory(), value))
            creature.setCategory(value);
        float health = creature.getHealth();
        if (dragFloat("health", health, kStatDragSpeed, kHealthMin, kHealthMax, "%.0f"))
            creature.setHealth(health);
        int level = creature.getLevel();
        if (dragInt("level", level, kIntDragSpeed, kSmallCountMin, kLevelMax))
            creature.setLevel(level);
        int rank = creature.getRank();
        if (dragInt("rank", rank, kIntDragSpeed, kSmallCountMin, kSmallCountMax))
            creature.setRank(rank);
        drawTransformEditors(creature, m_entityName, sizeof(m_entityName), player, "Teleport me here");
    }

    void CreaturesTab::drawNearby(cube::Player& player)
    {
        // Loader returns creatures nearest first, so index 0 is the closest creature.
        std::vector<cube::Creature> nearby = cube::creaturesOf(g_api);
        const int total = static_cast<int>(nearby.size());
        ImGui::Text("%d nearby (expand any row for full detail)", total);
        drawRelationLegend();
        // Show EVERY nearby creature the loader returned; a scroll region keeps the
        // full list usable no matter how many are loaded.
        ImGui::BeginChild("ent_list", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        for (int i = 0; i < total; ++i)
        {
            const cube::Creature& creature = nearby[static_cast<size_t>(i)];
            // Key the row by live creature address, never the loop index: the creature set reorders
            // every frame, so an index keyed node's open state + actions would hit the wrong creature.
            ImGui::PushID(static_cast<int>(creature.raw().address));
            ImGui::PushStyleColor(ImGuiCol_Text, creatureColor(creature));
            const bool open = ImGui::TreeNode("row", "%s  L%d  %.0fm  %s (kind %d)%s",
                                              creature.getName()[0] ? creature.getName() : "(unnamed)", creature.getLevel(),
                                              creature.getDistance(), creatureLabel(creature), creature.getCategory(),
                                              creature.isBoss() ? " BOSS" : "");
            ImGui::PopStyleColor();
            if (open)
            {
                drawCreatureDetail(creature, player);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        ImGui::EndChild();
    }

    void CreaturesTab::drawCompanion(cube::Player& player)
    {
        cube::Companion pet(g_api);
        if (!pet.valid())
        {
            ImGui::TextDisabled("no active pet");
            return;
        }
        if (beginTable("en_pet"))
        {
            row("Address", "0x%08X", pet.raw().address);
            row("XP", "%u", pet.getXp());
            row("State", "%s", cube::creatureStateName(pet.getState()));
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
        drawTransformEditors(pet, m_petName, sizeof(m_petName), player, "Teleport me to pet");
    }

    void CreaturesTab::draw(const CubeEventArgs&)
    {
        cube::Player player(g_api);
        if (!ImGui::BeginTabBar("##enttabs"))
            return;
        if (ImGui::BeginTabItem("Nearby"))
        {
            drawNearby(player);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Target"))
        {
            cube::Creature target;
            if (!cube::targetOf(g_api, target))
                ImGui::TextDisabled("no target");
            else
            {
                ImGui::PushID("tgt");
                drawCreatureDetail(target, player);
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Aim"))
        {
            ImGui::TextDisabled("crosshair hover target (what you are looking at)");
            cube::Creature aim;
            if (!cube::aimTargetOf(g_api, aim))
                ImGui::TextDisabled("not aiming at a creature");
            else
            {
                ImGui::PushID("aim");
                drawCreatureDetail(aim, player);
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Companion"))
        {
            drawCompanion(player);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

}
