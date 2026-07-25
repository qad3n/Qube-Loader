#include "api/bridge.h"
#include "loader/game/eventbacking.h"
#include "game/selection.h"
#include "cube_sdk.h"

#include <atomic>

namespace modloader::api
{
    namespace
    {
        // The R/use-key capture is not installed until something wants it. A mod that only polls
        // getLast still needs it armed, so the first call takes the hold, and that first call returns
        // "none" because nothing has been captured yet. Held for the session (a poller has no "done"
        // edge); released by eventbacking::releaseAll at teardown.
        std::atomic<bool> g_armed{false};

        void armOnFirstUse()
        {
            bool expected = false;
            if (g_armed.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                eventbacking::acquire(eventbacking::Backing::Selection);
        }

        int32_t CUBE_CALL apiSelectionGetLast(const CubeApi* api, CubeSelection* out)
        {
            armOnFirstUse();
            return bridgeGet<CubeSelection>(api, out, &game::selection::readLast, "selection.getLast", "none", &CubeSelection::address);
        }
    }

    void fillSelection(CubeApi& api)
    {
        api.selection.getLast = &apiSelectionGetLast;
    }
}
