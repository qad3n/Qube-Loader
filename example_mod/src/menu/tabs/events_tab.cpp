#include "menu/tabs/events_tab.h"
#include "features/game_events.h"

#include "imgui.h"

namespace exmod::menu
{
    namespace
    {
        constexpr float kEventLogHeight = 170.0f;
    }

    void EventsTab::drawLog()
    {
        GameEvents& events = gameEvents();
        if (ImGui::Button("Clear"))
            events.clearLog();

        ImGui::SameLine();
        ImGui::Checkbox("autoscroll", &m_autoScroll);
        ImGui::BeginChild("##evlog", ImVec2(0.0f, sc(kEventLogHeight)), ImGuiChildFlags_Borders);

        const int lines = events.logLineCount();
        for (int k = 0; k < lines; ++k)
            ImGui::TextUnformatted(events.logLineAt(k));

        if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
    }

    void EventsTab::draw(const CubeEventArgs&)
    {
        ImGui::TextWrapped("mod.eventListener OBSERVES: the loader detects each edge and delivers "
            "it here (read only; the game runs normally). To INTERCEPT a call instead, "
            "see the Hooks tab (mod.eventHook).");

        GameEvents& events = gameEvents();

        // EventListener::remove(Event) demo: detach/reattach one representative listener at runtime.
        bool areaChange = events.areaChangeListening();
        if (ImGui::Checkbox("subscribed to AreaChange (eventListener.remove then add back)", &areaChange))
            events.setAreaChangeListening(areaChange);

        ImGui::Separator();

        if (!ImGui::BeginTabBar("##evtabs"))
            return;

        if (ImGui::BeginTabItem("Live log"))
        {
            drawLog();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Counters"))
        {
            if (beginTable("ev_ct"))
            {
                for (int i = 0; i < CUBE_EVENT_COUNT; ++i)
                    row(events.nameAt(i), "%u", events.countAt(i));
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Console toggles"))
        {
            ImGui::TextDisabled("which events also print to the log:");
            for (int i = 0; i < CUBE_EVENT_COUNT; ++i)
            {
                ImGui::PushID(i);
                ImGui::Checkbox(events.nameAt(i), &events.consoleEnabled(i));
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Catalog"))
        {
            if (beginTable("ev_cat"))
            {
                for (int i = 0; i < CUBE_EVENT_COUNT; ++i)
                    row(events.nameAt(i), "%s", events.noteAt(i));
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
