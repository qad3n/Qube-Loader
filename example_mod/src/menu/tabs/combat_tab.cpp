#include "menu/tabs/combat_tab.h"
#include "mod_context.h"
#include "features/health_history.h"

#include "imgui.h"

#include <cfloat>
#include <vector>

namespace exmod::menu
{
    namespace
    {
        constexpr float kPlotHeight = 60.0f;
        constexpr int kHitStunMin = 0;
        constexpr int kHitStunMax = 600;
        constexpr float kAttackSpeedMin = 0.01f;
        constexpr float kAttackSpeedMax = 100.0f;
        constexpr float kCritMin = 0.0f;
        constexpr float kCritMax = 100.0f;
        constexpr float kCooldownMin = 0.0f;
        constexpr float kCooldownMax = 100000.0f;
        constexpr int kBuffDurMin = 0;
        constexpr int kBuffDurMax = 1000000000;
    }

    void CombatTab::drawStats(cube::Combat& combat, cube::Hero& hero)
    {
        addressHeader("Creature", combat.raw().address);
        float baseDamage = combat.getBaseDamage();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::DragFloat("base damage", &baseDamage, kStatDragSpeed, kStatMin, kStatMax, "%.1f", kClampFlags))
            hero.setBaseDamage(baseDamage);

        ImGui::SameLine();
        ImGui::TextDisabled("(scales every attack + ability)");

        float power = combat.getPower();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::DragFloat("power", &power, kStatDragSpeed, kStatMin, kStatMax, "%.1f", kClampFlags))
            hero.setPower(power);

        float armor = combat.getArmor();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::DragFloat("armor", &armor, kStatDragSpeed, kStatMin, kStatMax, "%.1f", kClampFlags))
            hero.setArmor(armor);

        float spirit = combat.getSpirit();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::DragFloat("spirit", &spirit, kStatDragSpeed, kStatMin, kStatMax, "%.1f", kClampFlags))
            hero.setSpirit(spirit);

        int combo = combat.getCombo();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::DragInt("combo", &combo, kIntDragSpeed, kSmallCountMin, kSmallCountMax, "%d", kClampFlags))
            hero.setCombo(combo);

        float attackSpeed = combat.getAttackSpeed();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::DragFloat("attack speed", &attackSpeed, kFineDragSpeed, kAttackSpeedMin, kAttackSpeedMax, "%.2f", kClampFlags))
            hero.setAttackSpeed(attackSpeed);

        float crit = combat.getCritStat();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::DragFloat("crit", &crit, kFineDragSpeed, kCritMin, kCritMax, "%.2f", kClampFlags))
            hero.setCrit(crit);

        ImGui::Text("crit chance ~%.1f%% (excludes gear bonus)", combat.getCritChancePercent());
    }

    void CombatTab::drawTelemetry(cube::Combat& combat, cube::Hero& hero)
    {
        addressHeader("Creature", combat.raw().address);

        if (beginTable("cmb_tel"))
        {
            row("Last dmg dealt", "%.0f", combat.getLastDamageDealt());
            row("Last dmg taken", "%.0f", combat.getLastDamageTaken());
            row("Hits", "%u", combat.getHits());
            row("Crits", "%u", combat.getCrits());
            row("Times damaged", "%u", combat.getDamageTakenEvents());
            ImGui::EndTable();
        }

        ImGui::TextDisabled("hits/last dmg poll any nearby creature's health drop;");
        ImGui::TextDisabled("crit counter needs a damage detour (see crit chance in Stats)");

        if (hero.valid())
        {
            float cooldown = combat.getAttackCooldown();
            ImGui::SetNextItemWidth(sc(kInputWidth));
            if (ImGui::DragFloat("attack cd", &cooldown, kFineDragSpeed, kCooldownMin, kCooldownMax, "%.2f", kClampFlags))
                hero.setAttackCooldown(cooldown);
            int hitStun = combat.getHitStun();
            ImGui::SetNextItemWidth(sc(kInputWidth));
            if (ImGui::DragInt("hit stun", &hitStun, kIntDragSpeed, kHitStunMin, kHitStunMax, "%d", kClampFlags))
                hero.setHitStun(hitStun);
            ImGui::SeparatorText("stun state");
            const cube::Stun stun = hero.getStun();
            ImGui::Text("stunned: %s   knocked down: %s", stun.isStunned() ? "yes" : "no",
                        stun.isKnockedDown() ? "yes" : "no");
            ImGui::Text("lock timer: %d (%.0f%%)", stun.getHitStun(), stun.getHitStunPercent());
            const cube::Vec3 kb = stun.getKnockback();
            ImGui::Text("knockback: %.1f, %.1f", kb.x, kb.y);
            if (ImGui::Button("Break free"))
                hero.clearStun();
        }
        ImGui::SeparatorText("health history");
        const HealthHistory& history = healthHistory();
        ImGui::PlotLines("##hp", history.data(), history.size(), history.head(), nullptr,
                         FLT_MAX, FLT_MAX, ImVec2(0.0f, sc(kPlotHeight)));
    }

    void CombatTab::drawBuffs()
    {
        const std::vector<cube::Buff> buffs = cube::buffsOf(g_api);
        if (buffs.empty())
        {
            ImGui::TextDisabled("no active effects");
            return;
        }
        ImGui::Text("%d active", static_cast<int>(buffs.size()));
        for (size_t i = 0; i < buffs.size(); ++i)
        {
            const cube::Buff& buff = buffs[i];
            ImGui::PushID(static_cast<int>(buff.raw().address));
            ImGui::TextDisabled("effect @ 0x%08X", buff.raw().address);

            int type = 0;
            if (idEditor("type", CUBE_CATALOG_BUFF_TYPE, buff.getType(), type))
                buff.setType(type);

            float magnitude = buff.getMagnitude();
            ImGui::SetNextItemWidth(sc(kInputWidth));
            if (ImGui::DragFloat("magnitude", &magnitude, kFineDragSpeed, kStatMin, kStatMax, "%.2f", kClampFlags))
                buff.setMagnitude(magnitude);

            int remaining = buff.getRemainingMs();
            ImGui::SetNextItemWidth(sc(kInputWidth));
            if (ImGui::DragInt("remaining ms", &remaining, kIntDragSpeed, kBuffDurMin, kBuffDurMax, "%d", kClampFlags))
                buff.setRemainingMs(remaining);

            ImGui::Separator();
            ImGui::PopID();
        }
    }

    void CombatTab::drawAbilities()
    {
        ImGui::TextDisabled("live per ability cooldown timers (0 = ready).");
        ImGui::TextDisabled("base cooldown durations are code formulas, not editable.");
        const std::vector<cube::AbilityCooldown> cooldowns = cube::abilityCooldownsOf(g_api);

        if (ImGui::Button("clear all cooldowns"))
            cube::mod().clearAbilityCooldowns();

        // Set a cooldown by ability id (mod.setAbilityCooldown), independent of the live list below.
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::DragInt("ability id##setcd", &m_cdAbilityId, kIntDragSpeed, kSmallCountMin, kSmallCountMax, "%d", kClampFlags);
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::DragInt("ms##setcd", &m_cdMs, kIntDragSpeed, kBuffDurMin, kBuffDurMax, "%d", kClampFlags);
        ImGui::SameLine();
        if (ImGui::Button("Set by id"))
            cube::mod().setAbilityCooldown(m_cdAbilityId, m_cdMs);

        if (cooldowns.empty())
        {
            ImGui::TextDisabled("no ability has been used yet this session");
            return;
        }

        if (beginTable("abil_cd"))
        {
            for (size_t i = 0; i < cooldowns.size(); ++i)
            {
                const cube::AbilityCooldown& cd = cooldowns[i];
                const char* name = cube::catalog::nameOr(g_api, CUBE_CATALOG_ABILITY, cd.getAbilityId(), "");

                ImGui::PushID(static_cast<int>(cd.getAbilityId()));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                if (name[0])
                    ImGui::Text("%s (#%d)", name, cd.getAbilityId());
                else
                    ImGui::Text("ability #%d", cd.getAbilityId());

                ImGui::TableNextColumn();
                int remaining = cd.getRemainingMs();
                ImGui::SetNextItemWidth(sc(kInputWidth));

                if (ImGui::DragInt("ms", &remaining, kIntDragSpeed, kBuffDurMin, kBuffDurMax, "%d", kClampFlags))
                    cd.setRemaining(remaining);

                ImGui::SameLine();

                if (ImGui::SmallButton("ready"))
                    cd.ready();

                if (cd.isReady())
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(ready)");
                }

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    void CombatTab::draw(const CubeEventArgs&)
    {
        cube::Combat combat(g_api);
        cube::Hero hero(g_api);

        if (!ImGui::BeginTabBar("##combattabs"))
            return;

        if (ImGui::BeginTabItem("Stats"))
        {
            if (!combat.valid() || !hero.valid())
                ImGui::TextDisabled("(unavailable)");
            else
                drawStats(combat, hero);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Telemetry"))
        {
            if (!combat.valid())
                ImGui::TextDisabled("(unavailable)");
            else
                drawTelemetry(combat, hero);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Buffs"))
        {
            drawBuffs();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Abilities"))
        {
            drawAbilities();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}