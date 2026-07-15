#include "menu/tabs/mod_tab.h"
#include "mod_context.h"
#include "overlay.h"
#include "features/memory_probe.h"

#include "cube_mod.hpp"
#include "imgui.h"

#include <cstdio>

namespace exmod::menu
{
    namespace
    {

        constexpr char kDefaultLogMessage[] = "hello from the mod menu";

        const char* const kLogLevelNames[] = {"Trace", "Debug", "Info", "Warn", "Error"};

    }

    ModTab::ModTab()
    {
        m_uiScale = overlay::uiScale();
        std::snprintf(m_log.message, sizeof(m_log.message), "%s", kDefaultLogMessage);
    }

    void ModTab::drawInfo(const CubeEventArgs& frame)
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (beginTable("mod_info"))
        {
            row("Environment", "%s", cube::mod().isClient() ? "client" : "server");
            row("Loader ABI", "%u", g_api->abiVersion);
            row("SDK ABI", "%d", CUBE_ABI_VERSION);
            row("FPS", "%.1f", io.Framerate);
            row("Frame", "%u", frame.frameIndex);
            row("DPI scale", "%.2fx", overlay::dpiScale());
            ImGui::EndTable();
        }
        ImGui::SeparatorText("ui scale");
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::SliderFloat("scale", &m_uiScale, overlay::kMinUiScale, overlay::kMaxUiScale, "%.2fx", kClampFlags))
            overlay::setUiScale(m_uiScale);
        ImGui::SameLine();
        if (ImGui::Button("Reset##scale"))
        {
            m_uiScale = 1.0f;
            overlay::setUiScale(m_uiScale);
        }
    }

    void ModTab::drawMemory()
    {
        // All parse/rebase/read/write logic lives in MemoryProbe; this view only binds + displays.
        MemoryProbe& probe = memoryProbe();
        ImGui::TextDisabled("mod.rebase / readable / read<T> (all loader guarded)");
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::InputText("static addr##mem", probe.addressBuffer(), probe.bufferSize(),
                         ImGuiInputTextFlags_CharsHexadecimal);
        const bool readable = probe.readable();
        if (beginTable("mod_mem"))
        {
            row("Static", "0x%08X", probe.staticAddress());
            row("Rebased", "0x%08X", probe.runtimeAddress());
            row("Readable", "%s", yesNo(readable));
            if (readable)
                row("u32 @addr", "0x%08X", probe.readU32());
            ImGui::EndTable();
        }

        ImGui::SeparatorText("mod.write<T> (guarded)");
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::InputText("value hex##memw", probe.valueBuffer(), probe.bufferSize(),
                         ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::Checkbox("arm write", &probe.writeArmed());
        ImGui::SameLine();
        ImGui::BeginDisabled(!probe.writeArmed());
        if (ImGui::Button("Write u32"))
        {
            const bool ok = probe.write();
            emitLog(ok ? CUBE_LOG_INFO : CUBE_LOG_WARN,
                    ok ? "example_mod: mod.write ok" : "example_mod: mod.write blocked");
        }
        ImGui::EndDisabled();
        ImGui::TextColored(kWarnColor, "Writes to the rebased address above. Be careful.");
    }

    void ModTab::drawLogging()
    {
        ImGui::TextDisabled("emit a line at any level via log.write");
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::Combo("level", &m_log.levelIndex, kLogLevelNames, IM_ARRAYSIZE(kLogLevelNames));
        ImGui::SetNextItemWidth(sc(kTeleportInputWidth));
        ImGui::InputText("message", m_log.message, sizeof(m_log.message));
        if (ImGui::Button("Log message"))
            emitLog(static_cast<CubeLogLevel>(m_log.levelIndex), m_log.message);

        ImGui::SeparatorText("quick");
        if (ImGui::Button("Trace"))
            emitLog(CUBE_LOG_TRACE, "example_mod: trace test line");
        ImGui::SameLine();
        if (ImGui::Button("Debug"))
            emitLog(CUBE_LOG_DEBUG, "example_mod: debug test line");
        ImGui::SameLine();
        if (ImGui::Button("Info"))
            emitLog(CUBE_LOG_INFO, "example_mod: info test line");
        ImGui::SameLine();
        if (ImGui::Button("Warn"))
            emitLog(CUBE_LOG_WARN, "example_mod: warn test line");
        ImGui::SameLine();
        if (ImGui::Button("Error"))
            emitLog(CUBE_LOG_ERROR, "example_mod: error test line");
    }

    void ModTab::draw(const CubeEventArgs& frame)
    {
        if (!ImGui::BeginTabBar("##modtabs"))
            return;
        if (ImGui::BeginTabItem("Info"))
        {
            drawInfo(frame);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Memory"))
        {
            drawMemory();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Logging"))
        {
            drawLogging();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

}
