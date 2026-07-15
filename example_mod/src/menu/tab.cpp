#include "menu/tab.h"
#include "mod_context.h"
#include "overlay.h"

#include "cube_mod.hpp"
#include "imgui.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

namespace exmod::menu
{
    namespace
    {

        constexpr int kTableColumns = 2;
        constexpr int kIdMin = 0;
        constexpr int kIdMax = 100000;

    }

    float Tab::sc(float px)
    {
        return px * overlay::dpiScale() * overlay::uiScale();
    }

    const char* Tab::yesNo(bool value)
    {
        return value ? "yes" : "no";
    }

    bool Tab::beginTable(const char* id)
    {
        return ImGui::BeginTable(id, kTableColumns, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg);
    }

    void Tab::row(const char* label, const char* fmt, ...)
    {
        char value[kValueBufferSize];
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(value, sizeof(value), fmt, args);
        va_end(args);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(label);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(value);
    }

    void Tab::addressHeader(const char* what, unsigned address)
    {
        ImGui::TextDisabled("%s @ 0x%08X", what, address);
        ImGui::Separator();
    }

    bool Tab::idEditor(const char* label, CubeCatalog catalog, int currentId, int& outId)
    {
        outId = currentId;
        const std::vector<cube::NamedValue> options = cube::catalog::options(g_api, catalog);
        if (options.empty())
        {
            ImGui::SetNextItemWidth(sc(kInputWidth));
            const bool changed = ImGui::DragInt(label, &outId, kIntDragSpeed, kIdMin, kIdMax, "%d", kClampFlags);
            return changed;
        }
        char preview[kValueBufferSize];
        std::snprintf(preview, sizeof(preview), "%d: %s", currentId,
                      cube::catalog::nameOr(g_api, catalog, currentId, "(unknown)"));
        bool changed = false;
        ImGui::SetNextItemWidth(sc(kComboWidth));
        if (ImGui::BeginCombo(label, preview))
        {
            for (const cube::NamedValue& option : options)
            {
                char optionLabel[kValueBufferSize];
                std::snprintf(optionLabel, sizeof(optionLabel), "%d: %s", option.id, option.name);
                if (ImGui::Selectable(optionLabel, option.id == currentId))
                {
                    outId = option.id;
                    changed = true;
                }
            }
            ImGui::EndCombo();
        }
        return changed;
    }

    void Tab::emitLog(CubeLogLevel level, const char* message)
    {
        exmod::logLine(level, message);
    }

}
