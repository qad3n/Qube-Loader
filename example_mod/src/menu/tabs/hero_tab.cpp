#include "menu/tabs/hero_tab.h"
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

    void HeroTab::drawVitals(cube::Hero& hero)
    {
        if (beginTable("hero_vit"))
        {
            row("Health", "%.0f%s", hero.getHealth(), hero.isAlive() ? "" : "  (dead)");
            row("Mana", "%.0f%% (%.3f raw)", hero.getManaPercent(), hero.getMana());
            row("Stamina", "%.0f%% (%.3f raw)", hero.getStaminaPercent(), hero.getStamina());
            ImGui::EndTable();
        }
        ImGui::SeparatorText("set live");
        float health = hero.getHealth();
        if (dragFloat("health##set", health, kHealthDragSpeed, kHealthMin, kHealthMax, "%.0f"))
            hero.setHealth(health);
        float mana = hero.getMana();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::SliderFloat("mana##set", &mana, kResourceMin, kFullResource, "%.2f", kClampFlags))
            hero.setMana(mana);
        float stamina = hero.getStamina();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::SliderFloat("stamina##set", &stamina, kResourceMin, kFullResource, "%.2f", kClampFlags))
            hero.setStamina(stamina);

        ImGui::SeparatorText("keep applied (per frame)");
        Cheats::Settings& cheat = cheats().settings();
        ImGui::Checkbox("god mode", &cheat.godMode);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::DragFloat("hp##god", &cheat.godModeHealth, kHealthDragSpeed, kHealthMin, kHealthMax, "%.0f", kClampFlags);
        ImGui::Checkbox("infinite mana", &cheat.infiniteMana);
        ImGui::Checkbox("infinite stamina", &cheat.infiniteStamina);
    }

    void HeroTab::drawProgress(cube::Hero& hero)
    {
        if (beginTable("hero_prog"))
        {
            row("Level", "%d", hero.getLevel());
            row("XP", "%u", hero.getXp());
            row("Coins", "%d", hero.getCoins());
            ImGui::EndTable();
        }
        ImGui::SeparatorText("actions");
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::DragInt("level##set", &m_inputs.level, kIntDragSpeed, kLevelMin, kLevelMax, "%d", kClampFlags);
        ImGui::SameLine();
        if (ImGui::Button("Set##lvl"))
            hero.setLevel(m_inputs.level);

        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::DragInt("xp amount##set", &m_inputs.xpAmount, kIntDragSpeed, kCountMin, kCountMax, "%d", kClampFlags);
        ImGui::SameLine();
        if (ImGui::Button("Give##xp"))
            hero.giveXp(m_inputs.xpAmount);

        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::DragInt("coins##set", &m_inputs.coins, kIntDragSpeed, kCountMin, kCountMax, "%d", kClampFlags);
        ImGui::SameLine();
        if (ImGui::Button("Set##coins"))
            hero.setCoins(m_inputs.coins);

        if (ImGui::Button("Give 100 XP"))
            hero.giveXp(kQuickXpSmall);
        ImGui::SameLine();
        if (ImGui::Button("Give 1000 XP"))
            hero.giveXp(kQuickXpLarge);
        ImGui::SameLine();
        if (ImGui::Button("+1000 coins"))
            hero.setCoins(hero.getCoins() + kQuickCoins);
    }

    void HeroTab::drawMovement(cube::Hero& hero)
    {
        const cube::Vec3 pos = hero.getPosition();
        const cube::Vec3 vel = hero.getVelocity();
        if (beginTable("hero_mov"))
        {
            row("Position", "%.1f, %.1f, %.1f", pos.x, pos.y, pos.z);
            row("Velocity", "%.2f, %.2f, %.2f", vel.x, vel.y, vel.z);
            row("Speed", "%.2f", hero.getSpeed());
            row("Facing", "%.2f rad", hero.getFacing());
            row("Movement", "%s", hero.getMovementText());
            row("Action", "%s (id %d)", hero.getActionText(), hero.getActionId());
            row("On ground / attacking", "%s / %s", yesNo(hero.isOnGround()), yesNo(hero.isAttacking()));
            row("Stealth (sneaking)", "%.2f (%s)", hero.getStealth(), yesNo(hero.isSneaking()));
            row("Lantern", "%s", yesNo(hero.hasLantern()));
            ImGui::EndTable();
        }
        ImGui::SeparatorText("teleport");
        ImGui::SetNextItemWidth(sc(kTeleportInputWidth));
        ImGui::InputFloat3("x y z##tp", m_inputs.teleport);
        if (ImGui::Button("Teleport"))
            hero.teleport(m_inputs.teleport[0], m_inputs.teleport[1], m_inputs.teleport[2]);
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
                hero.teleport(world.getSpawn());
        }
        ImGui::SeparatorText("facing + velocity");
        float facing = hero.getFacing();
        ImGui::SetNextItemWidth(sc(kTeleportInputWidth));
        if (ImGui::SliderAngle("facing##set", &facing))
            hero.setFacing(facing);
        float velocity[3] = {vel.x, vel.y, vel.z};
        ImGui::SetNextItemWidth(sc(kTeleportInputWidth));
        if (ImGui::DragFloat3("velocity##set", velocity, kFineDragSpeed))
            hero.setVelocity(velocity[0], velocity[1], velocity[2]);
        int actionValue = 0;
        if (idEditor("action", CUBE_CATALOG_ACTION, hero.getActionId(), actionValue))
            hero.setActionId(actionValue);
        bool sneaking = hero.isSneaking();
        if (ImGui::Checkbox("sneaking (stealth stat)", &sneaking))
            hero.setSneaking(sneaking);
        ImGui::SameLine();
        bool lantern = hero.hasLantern();
        if (ImGui::Checkbox("lantern", &lantern))
            hero.setLantern(lantern);
        ImGui::TextDisabled("stealth reduces enemy detection; lantern is the held light");
        ImGui::TextDisabled("speed = |velocity| (no move speed stat exists in this build)");
    }

    void HeroTab::drawIdentity(cube::Hero& hero)
    {
        if (beginTable("hero_id"))
        {
            row("Name", "%s", hero.getName()[0] ? hero.getName() : "(unresolved)");
            row("Class", "%s", hero.getClassName());
            row("Selected target", "0x%08X", hero.raw().target);
            row("Resolved", "name %s / pos %s / state %s", yesNo(hero.hasName()),
                yesNo(hero.hasPosition()), yesNo(hero.hasState()));
            ImGui::EndTable();
        }
        ImGui::TextDisabled("selected target is 0 until you press the use key (R) on a creature;");
        ImGui::TextDisabled("for the crosshair/aim target see the Entities > Aim tab.");
        ImGui::SeparatorText("edit");
        int value = 0;
        if (idEditor("class", CUBE_CATALOG_CLASS, static_cast<int>(hero.getClass()), value))
            hero.setClass(static_cast<cube::Class>(value));
        if (idEditor("type / race", CUBE_CATALOG_SPECIES, hero.getType(), value))
            hero.setType(value);
        int spec = hero.getSpec();
        if (dragInt("spec", spec, kIntDragSpeed, kSmallCountMin, kSmallCountMax))
            hero.setSpec(spec);
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::InputText("name##edit", m_name, sizeof(m_name));
        ImGui::SameLine();
        if (ImGui::Button("Set##name"))
            hero.setName(m_name);
    }

    void HeroTab::draw(const CubeEventArgs&)
    {
        cube::Hero hero(g_api);
        if (!hero.valid())
        {
            ImGui::TextDisabled("no player (spawn into a character)");
            return;
        }
        addressHeader("Creature", hero.raw().address);
        if (!ImGui::BeginTabBar("##herotabs"))
            return;
        if (ImGui::BeginTabItem("Vitals"))
        {
            drawVitals(hero);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Progress"))
        {
            drawProgress(hero);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Movement"))
        {
            drawMovement(hero);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Identity"))
        {
            drawIdentity(hero);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

}
