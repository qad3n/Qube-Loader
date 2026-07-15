#include "game/pickup.h"
#include "game/capture_detour.h"
#include "game/items.h"
#include "game/offsets.h"
#include "core/mem.h"

#include <cstdint>

namespace game::pickup
{
    namespace
    {
        constexpr char kCategory[] = "pickup";

        // GameController::onItemPickup is __thiscall (ECX=GC, no stack args).
        CaptureDetour<CubeItem> g_capture(kCategory, off::kOnItemPickupFn, "pickup capture",
            "pickup detour installed (E-key item pickup tracking active)",
            "pickup detour failed to install; item-pickup events disabled");

        // Reads the staged pickup POD (set by the game before onItemPickup). False on an empty/
        // unreadable staged cell (type 0). All reads are VirtualQuery-guarded via game::readItem.
        bool capture(uintptr_t gc, CubeItem& item)
        {
            const uintptr_t staged = gc + off::kStagedPickupItemOff;

            item.structSize = sizeof(CubeItem);
            if (!game::readItem(staged, item))
                return false;

            int32_t stack = 0;
            mem::read(staged + off::kItemStructSize, stack); // staged cell count @+0x118
            item.stack = stack > 0 ? stack : 1;
            item.slot = -1; // not an equipment slot
            item.present = 1;
            item.address = 0; // the staged buffer is transient (reused next pickup); no stable base
            return true;
        }

        void __fastcall detour(void* gc, void* edx)
        {
            g_capture.dispatch(gc, edx, capture);
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

    bool readLast(CubeItem& out)
    {
        return g_capture.readLast(out);
    }

    bool pollPickup(CubeItem& out)
    {
        return g_capture.poll(out);
    }

}
