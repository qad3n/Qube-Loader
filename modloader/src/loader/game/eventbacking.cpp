#include "loader/game/eventbacking.h"
#include "game/gamehooks/gamehooks.h"
#include "game/attackwatch.h"
#include "game/selection.h"
#include "game/pickup.h"
#include "core/log.h"

#include <cstdint>
#include <mutex>

namespace modloader::eventbacking
{
    namespace
    {
        constexpr char kCategory[] = "backing";
        constexpr int kBackingCount = static_cast<int>(Backing::Count);

        std::mutex g_mutex;
        int32_t g_refs[kBackingCount] = {};
        bool g_installed[kBackingCount] = {};

        const char* backingName(Backing backing)
        {
            switch (backing)
            {
                case Backing::Selection:
                    return "selection capture";
                case Backing::Pickup:
                    return "pickup capture";
                case Backing::CritRoll:
                    return "CRIT_ROLL observation";
                case Backing::BehaviorTick:
                    return "AI_BEHAVIOR_TICK observation";
                default:
                    return "?";
            }
        }

        // The actual patch. Each of these refuses on a mismatched Cube.exe build on its own
        // (CaptureDetour::install and armBuiltin both call signature::verifyTarget), so the build guard
        // lives here in the arm path rather than gating loader setup.
        bool installBacking(Backing backing)
        {
            switch (backing)
            {
                case Backing::Selection:
                    return game::selection::install();
                case Backing::Pickup:
                    return game::pickup::install();
                case Backing::CritRoll:
                    return game::gamehooks::acquireObservation(CUBE_HOOK_CRIT_ROLL);
                case Backing::BehaviorTick:
                    if (!game::gamehooks::acquireObservation(CUBE_HOOK_AI_BEHAVIOR_TICK))
                        return false;
                    game::attackwatch::setActive(true);
                    return true;
                default:
                    return false;
            }
        }

        void removeBacking(Backing backing)
        {
            switch (backing)
            {
                case Backing::Selection:
                    game::selection::remove();
                    return;
                case Backing::Pickup:
                    game::pickup::remove();
                    return;
                case Backing::CritRoll:
                    game::gamehooks::releaseObservation(CUBE_HOOK_CRIT_ROLL);
                    return;
                case Backing::BehaviorTick:
                    game::attackwatch::setActive(false);
                    game::gamehooks::releaseObservation(CUBE_HOOK_AI_BEHAVIOR_TICK);
                    return;
                default:
                    return;
            }
        }

        // Which backings an event needs; out must hold at least 1 entry. Returns how many were written.
        // Every event not listed is poll backed: it diffs guarded reads and patches nothing.
        int backingsFor(CubeEvent event, Backing* out)
        {
            switch (event)
            {
                case CUBE_EVENT_ENTITY_SELECTED:
                    out[0] = Backing::Selection;
                    return 1;
                case CUBE_EVENT_ITEM_PICKUP:
                    out[0] = Backing::Pickup;
                    return 1;
                case CUBE_EVENT_PLAYER_CRIT:
                    out[0] = Backing::CritRoll;
                    return 1;
                case CUBE_EVENT_PLAYER_ATTACK:
                    out[0] = Backing::BehaviorTick;
                    return 1;
                default:
                    return 0;
            }
        }

        constexpr int kMaxBackingsPerEvent = 1;
    }

    bool acquire(Backing backing)
    {
        const int index = static_cast<int>(backing);
        if (index < 0 || index >= kBackingCount)
            return false;

        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_refs[index]++ > 0)
                return g_installed[index];
        }

        const bool ok = installBacking(backing);

        std::lock_guard<std::mutex> lock(g_mutex);
        g_installed[index] = ok;
        if (!ok && --g_refs[index] < 0)
            g_refs[index] = 0;
        else if (ok)
            LOGC(Debug, kCategory, "armed %s (first user)", backingName(backing));
        return ok;
    }

    void release(Backing backing)
    {
        const int index = static_cast<int>(backing);
        if (index < 0 || index >= kBackingCount)
            return;

        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_refs[index] > 0 && --g_refs[index] > 0)
                return;
            g_refs[index] = 0;
            if (!g_installed[index])
                return;
            g_installed[index] = false;
        }

        removeBacking(backing);
        LOGC(Debug, kCategory, "removed %s (last user gone)", backingName(backing));
    }

    void retainEvent(CubeEvent event)
    {
        Backing needed[kMaxBackingsPerEvent];
        const int count = backingsFor(event, needed);
        for (int i = 0; i < count; ++i)
            acquire(needed[i]);
    }

    void releaseEvent(CubeEvent event)
    {
        Backing needed[kMaxBackingsPerEvent];
        const int count = backingsFor(event, needed);
        for (int i = 0; i < count; ++i)
            release(needed[i]);
    }

    void releaseAll()
    {
        for (int i = 0; i < kBackingCount; ++i)
        {
            bool installed = false;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_refs[i] = 0;
                installed = g_installed[i];
                g_installed[i] = false;
            }
            if (installed)
                removeBacking(static_cast<Backing>(i));
        }
    }
}
