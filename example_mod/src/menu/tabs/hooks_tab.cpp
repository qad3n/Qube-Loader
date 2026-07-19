#include "menu/tabs/hooks_tab.h"
#include "features/game_hooks.h"

#include "cube_mod.hpp"
#include "imgui.h"

namespace exmod::menu
{
    namespace
    {

        constexpr float kHookScaleMin = 0.0f;
        constexpr float kHookScaleMax = 100.0f;
        const char* const kCallConvNames[] = {"thiscall", "cdecl"};

        // Detach and reattach checkbox for one built in hook (shows EventHook::remove(Hook) plus adding it back).
        void attachToggle(GameHooks& hooks, cube::Hook hook, const char* id)
        {
            bool attached = hooks.builtinAttached(hook);
            ImGui::PushID(id);
            if (ImGui::Checkbox("handler attached", &attached))
                hooks.setBuiltinAttached(hook, attached);
            ImGui::PopID();
        }

    }

    void HooksTab::drawBuiltin()
    {
        GameHooks& hooks = gameHooks();
        GameHooks::Settings& settings = hooks.settings();

        ImGui::TextWrapped("mod.eventHook INTERCEPTS a real game function on the game thread and "
                           "can cancel it, change its arguments, or override its return. The "
                           "Events tab (mod.eventListener) only observes. These toggles are "
                           "applied by handlers in GameHooks, so act in the game to see them.");
        ImGui::TextColored(kWarnColor, "handlers run on the game thread; keep them tiny");
        ImGui::Separator();

        ImGui::TextDisabled("Impact (an incoming hit): cancel it, or rescale its damage");
        attachToggle(hooks, cube::Hook::Impact, "att_impact");
        ImGui::Checkbox("invincible (cancel every hit)", &settings.cancelImpact);
        ImGui::Checkbox("rescale incoming damage", &settings.scaleDamage);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::SliderFloat("x##dmg", &settings.damageScale, kHookScaleMin, kHookScaleMax, "%.2fx", kClampFlags);
        ImGui::Text("hits intercepted: %u", hooks.impactHits());
        ImGui::Separator();

        ImGui::TextDisabled("Crit roll: force or deny critical hits");
        // The crit slot is borrowed by the rawDetour demo (Raw tab) while installed.
        ImGui::BeginDisabled(hooks.rawDetourInstalled());
        attachToggle(hooks, cube::Hook::CritRoll, "att_crit");
        ImGui::EndDisabled();
        ImGui::Checkbox("force crit", &settings.forceCrit);
        ImGui::SameLine();
        ImGui::Checkbox("deny crit", &settings.denyCrit);
        ImGui::Text("crit rolls intercepted: %u", hooks.critRolls());
        ImGui::Separator();

        ImGui::TextDisabled("Max health: rescale the computed max HP (god mode or one shot)");
        attachToggle(hooks, cube::Hook::MaxHealth, "att_maxhp");
        ImGui::Checkbox("rescale max HP", &settings.scaleMaxHp);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::SliderFloat("x##maxhp", &settings.maxHpScale, kHookScaleMin, kHookScaleMax, "%.2fx", kClampFlags);
        ImGui::Text("max HP computations intercepted: %u", hooks.maxHpCalls());
        ImGui::Separator();

        // HookCall inspection: read what the last intercepted impact carried (addresses + args; the
        // guarded self/target memory reads use the same tool shown in the Mod tab's Memory sub tab).
        ImGui::TextDisabled("Last impact (HookCall inspection):");
        if (!hooks.hasLastImpact())
            ImGui::TextDisabled("  Nothing yet. Take or deal a hit to see one.");
        else if (beginTable("hk_last"))
        {
            row("attacker (self)", "0x%08X", hooks.lastImpactSelf());
            row("victim (target)", "0x%08X", hooks.lastImpactTarget());
            row("damage arg", "%d", hooks.lastImpactDamage());
            ImGui::EndTable();
        }
    }

    void HooksTab::drawRaw()
    {
        GameHooks& hooks = gameHooks();

        ImGui::TextWrapped("No built in hook for what you want? Find the function address yourself "
                           "and hook it the SAME way with mod.eventHook.raw(addr, cc, argCount, fn). "
                           "You still get a HookCall, no detour to write. Give the address, calling "
                           "convention, and integer argument count.");
        ImGui::Separator();

        GameHooks::RawInput& raw = hooks.rawInput();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::InputText("address (hex)", raw.address, sizeof(raw.address),
                         hooks.rawInstalled() ? ImGuiInputTextFlags_ReadOnly : 0);
        ImGui::SetNextItemWidth(sc(kComboWidth));
        ImGui::Combo("convention", &raw.conv, kCallConvNames,
                     static_cast<int>(sizeof(kCallConvNames) / sizeof(kCallConvNames[0])));
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::SliderInt("int args", &raw.argCount, 0, CUBE_HOOK_ARG_MAX, "%d", kClampFlags);

        if (!hooks.rawInstalled())
        {
            if (ImGui::Button("Install raw hook") && !hooks.installRawFromInput())
                emitLog(CUBE_LOG_WARN, "example_mod: raw hook failed (unsupported convention or pool full)");
        }
        else if (ImGui::Button("Remove raw hook"))
            hooks.removeRawFromInput();

        ImGui::SameLine();
        ImGui::TextDisabled(hooks.rawInstalled() ? "installed" : "not installed");
        ImGui::Text("raw calls intercepted: %u", hooks.rawCalls());
        ImGui::TextColored(kWarnColor, "advanced: a wrong address/convention can crash the game");

        ImGui::SeparatorText("manual detour (rawDetour)");
        ImGui::TextWrapped("For float/struct returns or odd arity the generic raw path cannot model, "
                           "write your own detour and take the trampoline via mod.eventHook.rawDetour. "
                           "This demo hooks the crit roll function with a hand written detour "
                           "(temporarily replacing the built in crit hook above).");
        if (!hooks.rawDetourInstalled())
        {
            if (ImGui::Button("Install manual detour"))
            {
                if (!hooks.installRawDetour())
                    emitLog(CUBE_LOG_WARN, "example_mod: rawDetour install failed (crit slot busy)");
            }
        }
        else if (ImGui::Button("Remove manual detour"))
            hooks.removeRawDetour();
        ImGui::SameLine();
        ImGui::TextDisabled(hooks.rawDetourInstalled() ? "installed" : "not installed");
        ImGui::Text("detour calls: %u", hooks.rawDetourCalls());
    }

    void HooksTab::draw(const CubeEventArgs&)
    {
        if (!ImGui::BeginTabBar("##hooktabs"))
            return;

        if (ImGui::BeginTabItem("Built in"))
        {
            drawBuiltin();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Raw address"))
        {
            drawRaw();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

}
