#include "menu/tabs/session_tab.h"
#include "mod_context.h"

#include "cube_mod.hpp"
#include "imgui.h"

namespace exmod::menu
{

    void SessionTab::drawState()
    {
        cube::Session session(g_api);
        if (!session.valid())
        {
            ImGui::TextDisabled("(unavailable)");
            return;
        }
        addressHeader("GameController", session.getAddress());
        if (beginTable("se_state"))
        {
            row("State", "%s (from resolution)", session.isInWorld() ? "in world" : "title");
            row("Players", "%d (counted)", session.getPlayerCount());
            ImGui::EndTable();
        }
        if (session.hasNetwork())
        {
            bool multiplayer = session.isMultiplayer();
            if (ImGui::Checkbox("multiplayer", &multiplayer))
                session.setMultiplayer(multiplayer);
            bool connected = session.isConnected();
            if (ImGui::Checkbox("connected", &connected))
                session.setConnected(connected);
            ImGui::TextColored(kWarnColor, "editing network state mid game may destabilize");
        }
    }

    void SessionTab::drawUi()
    {
        cube::Ui ui(g_api);
        if (!ui.valid())
        {
            ImGui::TextDisabled("(unavailable)");
            return;
        }
        addressHeader("GameController", ui.getAddress());
        bool inventory = ui.isInventoryOpen();
        if (ImGui::Checkbox("inventory", &inventory))
            ui.setInventoryOpen(inventory);
        bool character = ui.isCharacterOpen();
        if (ImGui::Checkbox("character", &character))
            ui.setCharacterOpen(character);
        bool map = ui.isMapOpen();
        if (ImGui::Checkbox("map", &map))
            ui.setMapOpen(map);
        bool objective = ui.isObjectiveOpen();
        if (ImGui::Checkbox("objective", &objective))
            ui.setObjectiveOpen(objective);
        ImGui::TextDisabled("any menu open: %s", yesNo(ui.isMenuOpen()));
    }

    void SessionTab::drawSelection()
    {
        ImGui::TextDisabled("the last entity selected (R / use key), captured by a detour");
        cube::Selection sel(g_api);
        if (!sel.valid())
        {
            ImGui::TextDisabled("Nothing selected. Press R on a chest, NPC, or object.");
            return;
        }
        if (beginTable("sess_sel"))
        {
            row("Kind", "%s", sel.getKindName());
            row("Target", "0x%08X %s", sel.getAddress(), sel.getAddress() ? "(creature)" : "(world object)");
            row("Type byte", "0x%02X", sel.getTypeByte());
            row("Total selections", "%u", sel.getCount());
            ImGui::EndTable();
        }
        ImGui::TextDisabled("mods: mod.onEntitySelected([](unsigned addr, cube::SelectionKind k){...})");
    }

    void SessionTab::draw(const CubeEventArgs&)
    {
        if (!ImGui::BeginTabBar("##sesstabs"))
            return;
        if (ImGui::BeginTabItem("State"))
        {
            drawState();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("UI"))
        {
            drawUi();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Selection"))
        {
            drawSelection();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

}
