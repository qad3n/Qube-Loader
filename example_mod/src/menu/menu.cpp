#include "menu/menu.h"
#include "menu/tab.h"
#include "menu/tabs/hero_tab.h"
#include "menu/tabs/combat_tab.h"
#include "menu/tabs/items_tab.h"
#include "menu/tabs/world_tab.h"
#include "menu/tabs/entities_tab.h"
#include "menu/tabs/view_tab.h"
#include "menu/tabs/session_tab.h"
#include "menu/tabs/events_tab.h"
#include "menu/tabs/hooks_tab.h"
#include "menu/tabs/mod_tab.h"

#include "imgui.h"

#include <cfloat>

namespace exmod::menu
{
    namespace
    {

        constexpr char kWindowTitle[] = "Cube Mod Menu";
        constexpr float kWindowWidth = 680.0f;
        constexpr float kWindowHeight = 600.0f;
        constexpr float kMinWindowWidth = 420.0f;
        constexpr float kMinWindowHeight = 320.0f;
        constexpr float kSidebarWidth = 108.0f;
        constexpr int kMainTabCount = 10;

        // The whole menu: a main tab sidebar plus the active tab's content. Owns one instance of every
        // tab and dispatches draw() to the selected one. Pure view: no game state, no logic (the
        // feature classes own that).
        class Menu
        {
        public:
            void draw(const CubeEventArgs& frame);

        private:
            void drawSidebar();

            HeroTab m_hero;
            CombatTab m_combat;
            ItemsTab m_items;
            WorldTab m_world;
            EntitiesTab m_entities;
            ViewTab m_view;
            SessionTab m_session;
            EventsTab m_events;
            HooksTab m_hooks;
            ModTab m_mod;
            Tab* const m_tabs[kMainTabCount] = {&m_hero, &m_combat, &m_items, &m_world, &m_entities,
                                                &m_view, &m_session, &m_events, &m_hooks, &m_mod};
            int m_active = 0;
        };

        void Menu::drawSidebar()
        {
            ImGui::BeginChild("##sidebar", ImVec2(Tab::sc(kSidebarWidth), 0.0f), ImGuiChildFlags_Borders);
            for (int i = 0; i < kMainTabCount; ++i)
            {
                if (ImGui::Selectable(m_tabs[i]->label(), m_active == i))
                    m_active = i;
            }
            ImGui::EndChild();
        }

        void Menu::draw(const CubeEventArgs& frame)
        {
            ImGui::SetNextWindowSize(ImVec2(Tab::sc(kWindowWidth), Tab::sc(kWindowHeight)), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(Tab::sc(kMinWindowWidth), Tab::sc(kMinWindowHeight)),
                                                ImVec2(FLT_MAX, FLT_MAX));
            if (ImGui::Begin(kWindowTitle))
            {
                drawSidebar();
                ImGui::SameLine();
                ImGui::BeginChild("##content", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing()));
                m_tabs[m_active]->draw(frame);
                ImGui::EndChild();
                ImGui::Separator();
                ImGui::TextDisabled("INSERT / DELETE toggles this window (game input is frozen while open)");
            }
            ImGui::End();
        }

        Menu& menu()
        {
            static Menu g_menu;
            return g_menu;
        }

    }

    void draw(const CubeEventArgs* frame)
    {
        menu().draw(*frame);
    }

}
