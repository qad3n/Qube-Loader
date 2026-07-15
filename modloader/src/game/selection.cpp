#include "game/selection.h"
#include "game/capture_detour.h"
#include "game/offsets.h"
#include "core/mem.h"

#include <cstdint>

namespace game::selection
{
    namespace
    {
        constexpr char kCategory[] = "select";

        struct Record
        {
            uint32_t address;
            int32_t type;
        };

        // GameController::updateSelectedEntity is __thiscall (ECX=GC, no stack args).
        CaptureDetour<Record> g_capture(kCategory, off::kUpdateSelectedEntityFn, "select capture",
            "select detour installed (R/use-key selection tracking active)",
            "select detour failed to install; selection events disabled");

        CubeSelectionKind classify(uint32_t address, int32_t typeByte)
        {
            if (address == 0)
                return CUBE_SELECTION_WORLD;
            if (typeByte >= off::kSelectContainerLo && typeByte <= off::kSelectContainerHi)
                return CUBE_SELECTION_CONTAINER;
            if (typeByte == off::kSelectDialog)
                return CUBE_SELECTION_DIALOG;
            if (typeByte == off::kSelectWidget)
                return CUBE_SELECTION_WIDGET;
            return CUBE_SELECTION_CREATURE;
        }

        // Reads the committed target (set by updateSelectedEntity). Always publishes (a world
        // selection has target 0). All reads are VirtualQuery-guarded.
        bool capture(uintptr_t gc, Record& rec)
        {
            uint32_t target = 0;
            mem::read(gc + off::kSelectedEntityOff, target);
            int32_t typeByte = 0;

            if (target)
            {
                uint8_t discriminator = 0;
                if (mem::read(static_cast<uintptr_t>(target) + off::kPlayerClassOff, discriminator))
                    typeByte = static_cast<int32_t>(discriminator);
            }

            rec.address = target;
            rec.type = typeByte;
            return true;
        }

        void __fastcall detour(void* gc, void* edx)
        {
            g_capture.dispatch(gc, edx, capture);
        }

        bool fill(const Record& rec, CubeSelection& out)
        {
            out.structSize = sizeof(CubeSelection);
            out.address = rec.address;
            out.typeByte = rec.type;
            out.kind = static_cast<int32_t>(classify(rec.address, rec.type));
            out.count = static_cast<int32_t>(g_capture.count());
            out.valid = 1;
            return true;
        }

    }

    bool install()
    {
        return g_capture.install(reinterpret_cast<void*>(&detour));
    }

    void remove()
    {
        g_capture.remove();
    }

    bool readLast(CubeSelection& out)
    {
        Record rec = {};
        return g_capture.readLast(rec) && fill(rec, out);
    }

    bool pollSelection(CubeSelection& out)
    {
        Record rec = {};
        return g_capture.poll(rec) && fill(rec, out);
    }

}
