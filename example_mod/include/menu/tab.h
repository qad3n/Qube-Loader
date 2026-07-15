#pragma once
// Base class for every main tab plus the shared drawing toolkit (tables, scaled widths, id
// dropdowns) and layout/clamp constants. The Menu owns one instance of each and dispatches draw().

#include "cube_sdk.h"

#include "imgui.h"

namespace exmod::menu
{

    // Shared layout widths (pixels, before DPI/user scaling via Tab::sc).
    constexpr float kInputWidth = 130.0f;
    constexpr float kComboWidth = 190.0f;
    constexpr float kTeleportInputWidth = 220.0f;

    // Shared drag/format helpers.
    constexpr int kValueBufferSize = 192;
    constexpr int kClampFlags = ImGuiSliderFlags_AlwaysClamp;
    constexpr float kIntDragSpeed = 1.0f; // per pixel step for integer drags
    constexpr float kFineDragSpeed = 0.1f;
    constexpr float kStatDragSpeed = 1.0f;
    // Shared clamp bounds so live edits cannot set impossible/crashing values. Every
    // editor passes kClampFlags (ImGuiSliderFlags_AlwaysClamp) so typed input clamps too.
    constexpr float kResourceMin = 0.0f; // floor for 0..1 resource sliders
    constexpr float kFullResource = 1.0f; // ceiling for 0..1 resource sliders / full value
    constexpr float kHealthMin = 0.0f;
    constexpr float kHealthMax = 1000000.0f;
    constexpr float kStatMin = 0.0f;
    constexpr float kStatMax = 100000.0f;
    constexpr int kLevelMin = 1;
    constexpr int kLevelMax = 9999;
    constexpr int kSmallCountMin = 0; // combo/rank/upgrades/stack/skill
    constexpr int kSmallCountMax = 9999;

    const ImVec4 kWarnColor = ImVec4(1.0f, 0.45f, 0.45f, 1.0f);

    class Tab
    {
    public:
        virtual ~Tab() = default;

        virtual const char* label() const = 0;
        virtual void draw(const CubeEventArgs& frame) = 0;

        // Scales a pixel size by the effective DPI/font scale so the layout holds up on any display.
        // Public because the Menu scales its own window/sidebar chrome through it.
        static float sc(float px);

    protected:
        static const char* yesNo(bool value);
        // A two column fixed fit table used for every read only key/value panel.
        static bool beginTable(const char* id);
        static void row(const char* label, const char* fmt, ...);
        // A "backed by <addr>" header shown at the top of a section so every panel
        // reports the live memory address behind its data.
        static void addressHeader(const char* what, unsigned address);
        // Edits an opaque game id as a named dropdown (when the catalog has names) or a
        // clamped numeric field (when it does not). Returns true and fills outId on a change.
        static bool idEditor(const char* label, CubeCatalog catalog, int currentId, int& outId);
        static void emitLog(CubeLogLevel level, const char* message);
    };

}
