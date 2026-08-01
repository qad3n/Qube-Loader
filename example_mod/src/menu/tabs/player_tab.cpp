#include "menu/tabs/player_tab.h"
#include "mod_context.h"
#include "features/cheats.h"

#include "imgui.h"

namespace exmod::menu
{
    namespace
    {

        constexpr float kHealthDragSpeed = 1.0f;
        constexpr int kQuickXpLarge = 1000;
        constexpr int kCountMin = 0; // coins / xp / generic non negative
        constexpr int kCountMax = 1000000000; // < INT_MAX, safe headroom

    }

    void PlayerTab::drawVitals(cube::Player& player)
    {
        if (beginTable("hero_vit"))
        {
            row("Health", "%.0f%s", player.getHealth(), player.isAlive() ? "" : "  (dead)");
            row("Mana", "%.0f%% (%.3f raw)", player.getManaPercent(), player.getMana());
            row("Stamina", "%.0f%% (%.3f raw)", player.getStaminaPercent(), player.getStamina());
            ImGui::EndTable();
        }
        ImGui::SeparatorText("set live");
        float health = player.getHealth();
        if (dragFloat("health##set", health, kHealthDragSpeed, kHealthMin, kHealthMax, "%.0f"))
            player.setHealth(health);
        float mana = player.getMana();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::SliderFloat("mana##set", &mana, kResourceMin, kFullResource, "%.2f", kClampFlags))
            player.setMana(mana);
        float stamina = player.getStamina();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::SliderFloat("stamina##set", &stamina, kResourceMin, kFullResource, "%.2f", kClampFlags))
            player.setStamina(stamina);

        ImGui::SeparatorText("keep applied (per frame)");
        Cheats::Settings& cheat = cheats().settings();
        ImGui::Checkbox("god mode", &cheat.godMode);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::DragFloat("hp##god", &cheat.godModeHealth, kHealthDragSpeed, kHealthMin, kHealthMax, "%.0f",
                         kClampFlags);
        ImGui::Checkbox("infinite mana", &cheat.infiniteMana);
        ImGui::Checkbox("infinite stamina", &cheat.infiniteStamina);
    }

    void PlayerTab::drawProgress(cube::Player& player)
    {
        if (beginTable("hero_prog"))
        {
            row("Level", "%d", player.getLevel());
            row("XP", "%u", player.getXp());
            row("Coins", "%d", player.getCoins());
            ImGui::EndTable();
        }
        ImGui::SeparatorText("actions");
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::DragInt("level##set", &m_inputs.level, kIntDragSpeed, kLevelMin, kLevelMax, "%d", kClampFlags);
        ImGui::SameLine();
        if (ImGui::Button("Set##lvl"))
            player.setLevel(m_inputs.level);

        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::DragInt("xp amount##set", &m_inputs.xpAmount, kIntDragSpeed, kCountMin, kCountMax, "%d",
                       kClampFlags);
        ImGui::SameLine();
        if (ImGui::Button("Give##xp"))
            player.giveXp(m_inputs.xpAmount);

        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::DragInt("coins##set", &m_inputs.coins, kIntDragSpeed, kCountMin, kCountMax, "%d", kClampFlags);
        ImGui::SameLine();
        if (ImGui::Button("Set##coins"))
            player.setCoins(m_inputs.coins);

        if (ImGui::Button("Give 100 XP"))
            player.giveXp(kQuickXpSmall);
        ImGui::SameLine();
        if (ImGui::Button("Give 1000 XP"))
            player.giveXp(kQuickXpLarge);
        ImGui::SameLine();
        if (ImGui::Button("+1000 coins"))
            player.setCoins(player.getCoins() + kQuickCoins);
    }

    void PlayerTab::drawMovement(cube::Player& player)
    {
        const cube::Vec3 pos = player.getPosition();
        const cube::Vec3 vel = player.getVelocity();
        if (beginTable("hero_mov"))
        {
            row("Position", "%.1f, %.1f, %.1f", pos.x, pos.y, pos.z);
            row("Velocity", "%.2f, %.2f, %.2f", vel.x, vel.y, vel.z);
            row("Speed", "%.2f", player.getSpeed());
            row("Facing", "%.2f rad", player.getFacing());
            row("Movement", "%s", player.getMovementText());
            row("Action", "%s (id %d)", player.getActionText(), player.getActionId());
            row("On ground / attacking", "%s / %s", yesNo(player.isOnGround()), yesNo(player.isAttacking()));
            row("Stealth (sneaking)", "%.2f (%s)", player.getStealth(), yesNo(player.isSneaking()));
            row("Lantern", "%s", yesNo(player.hasLantern()));
            ImGui::EndTable();
        }
        ImGui::SeparatorText("teleport");
        ImGui::SetNextItemWidth(sc(kTeleportInputWidth));
        ImGui::InputFloat3("x y z##tp", m_inputs.teleport);
        if (ImGui::Button("Teleport"))
            player.teleport(m_inputs.teleport[0], m_inputs.teleport[1], m_inputs.teleport[2]);
        ImGui::SameLine();
        if (ImGui::Button("Fill current"))
        {
            m_inputs.teleport[0] = pos.x;
            m_inputs.teleport[1] = pos.y;
            m_inputs.teleport[2] = pos.z;
        }
        ImGui::SameLine();
        if (ImGui::Button("To spawn"))
        {
            cube::World world(g_api);
            if (world.hasSpawn())
                player.teleport(world.getSpawn());
        }
        ImGui::SeparatorText("facing + velocity");
        float facing = player.getFacing();
        ImGui::SetNextItemWidth(sc(kTeleportInputWidth));
        if (ImGui::SliderAngle("facing##set", &facing))
            player.setFacing(facing);
        float velocity[3] = {vel.x, vel.y, vel.z};
        ImGui::SetNextItemWidth(sc(kTeleportInputWidth));
        if (ImGui::DragFloat3("velocity##set", velocity, kFineDragSpeed))
            player.setVelocity(velocity[0], velocity[1], velocity[2]);
        int actionValue = 0;
        if (idEditor("action", CUBE_CATALOG_ACTION, player.getActionId(), actionValue))
            player.setActionId(actionValue);
        bool sneaking = player.isSneaking();
        if (ImGui::Checkbox("sneaking (stealth stat)", &sneaking))
            player.setSneaking(sneaking);
        ImGui::SameLine();
        bool lantern = player.hasLantern();
        if (ImGui::Checkbox("lantern", &lantern))
            player.setLantern(lantern);
        ImGui::TextDisabled("stealth reduces enemy detection; lantern is the held light");
        ImGui::TextDisabled("speed = |velocity| (no move speed stat exists in this build)");
    }

    void PlayerTab::drawIdentity(cube::Player& player)
    {
        if (beginTable("hero_id"))
        {
            row("Name", "%s", player.getName()[0] ? player.getName() : "(unresolved)");
            row("Class", "%s", player.getClassName());
            row("Selected target", "0x%08X", player.raw().target);
            row("Resolved", "name %s / pos %s / state %s", yesNo(player.hasName()),
                yesNo(player.hasPosition()), yesNo(player.hasState()));
            ImGui::EndTable();
        }

        // Appearance record (read only), exercised on both tiers: the cube::Appearance wrapper here,
        // and the raw g_api->appearance.of straight off the C ABI just below.
        cube::Appearance appearance(g_api);
        if (appearance.valid() && beginTable("hero_appear"))
        {
            row("Species", "%d", appearance.getSpecies());
            row("Gender", "%s", appearance.isFemale() ? "female" : "male");
            row("Style", "0x%08X", appearance.getStyle());
            row("Colors", "%d / %d / %d", appearance.getColor0(), appearance.getColor1(),
                appearance.getColor2());
            ImGui::EndTable();
        }
        CubeAppearance rawAppearance = {};
        if (g_api->appearance.of(g_api, player.raw().address, &rawAppearance))
            ImGui::TextDisabled("raw appearance.of(0x%08X) -> species %d", player.raw().address,
                                rawAppearance.species);
        ImGui::TextDisabled("appearance is read only; edits persist to the character save");
        ImGui::TextDisabled("selected target is 0 until you press the use key (R) on a creature;");
        ImGui::TextDisabled("for the crosshair/aim target see the Entities > Aim tab.");
        ImGui::SeparatorText("edit");
        int value = 0;
        if (idEditor("class", CUBE_CATALOG_CLASS, static_cast<int>(player.getClass()), value))
            player.setClass(static_cast<cube::Class>(value));
        if (idEditor("type / race", CUBE_CATALOG_SPECIES, player.getType(), value))
            player.setType(value);
        int spec = player.getSpec();
        if (dragInt("spec", spec, kIntDragSpeed, kSmallCountMin, kSmallCountMax))
            player.setSpec(spec);
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::InputText("name##edit", m_name, sizeof(m_name));
        ImGui::SameLine();
        if (ImGui::Button("Set##name"))
            player.setName(m_name);
    }

    void PlayerTab::draw(const CubeEventArgs&)
    {
        cube::Player player(g_api);
        if (!player.valid())
        {
            ImGui::TextDisabled("no player (spawn into a character)");
            return;
        }
        addressHeader("Creature", player.raw().address);
        if (!ImGui::BeginTabBar("##herotabs"))
            return;
        if (ImGui::BeginTabItem("Vitals"))
        {
            drawVitals(player);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Progress"))
        {
            drawProgress(player);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Movement"))
        {
            drawMovement(player);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Identity"))
        {
            drawIdentity(player);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

}
