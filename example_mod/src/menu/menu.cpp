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
        constexpr float kMaxWindowFraction = 0.9f; // cap the menu to 90% of the screen at any resolution
        constexpr float kUnboundedSize = 100000.0f; // fallback max before DisplaySize is known

        float minf(float a, float b)
        {
            return a < b ? a : b;
        }

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
            ImVec2 m_lastDisplay = {0.0f, 0.0f}; // re-fit the window when the resolution changes
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
            // Fit any resolution / aspect ratio: DisplaySize tracks the live backbuffer each frame, so
            // cap the max size to the screen and the min size so a small screen with a large DPI/scale
            // can never force the window bigger than the display.
            const ImVec2 disp = ImGui::GetIO().DisplaySize;
            const bool haveDisp = disp.x > 0.0f && disp.y > 0.0f;
            const float maxW = haveDisp ? disp.x : kUnboundedSize;
            const float maxH = haveDisp ? disp.y : kUnboundedSize;
            const float minW = minf(Tab::sc(kMinWindowWidth), maxW * kMaxWindowFraction);
            const float minH = minf(Tab::sc(kMinWindowHeight), maxH * kMaxWindowFraction);
            ImGui::SetNextWindowSize(ImVec2(Tab::sc(kWindowWidth), Tab::sc(kWindowHeight)), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(minW, minH), ImVec2(maxW, maxH));

            // On a resolution change (device reset) re-center into the new bounds so a downscale can
            // never leave the window stranded off-screen.
            if (haveDisp && (disp.x != m_lastDisplay.x || disp.y != m_lastDisplay.y))
            {
                m_lastDisplay = disp;
                const float w = minf(Tab::sc(kWindowWidth), disp.x);
                const float h = minf(Tab::sc(kWindowHeight), disp.y);
                ImGui::SetNextWindowPos(ImVec2((disp.x - w) * 0.5f, (disp.y - h) * 0.5f), ImGuiCond_Always);
            }

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
