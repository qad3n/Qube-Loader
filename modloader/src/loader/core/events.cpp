#include "loader/core/events.h"
#include "loader/core/owner_name.h"
#include "loader/core/registry.h"
#include "loader/game/eventbacking.h"
#include "core/log.h"
#include "core/faultguard.h"
#include "util/guard.h"
#include "util/inflight.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace modloader::events
{
    namespace
    {

        constexpr char kCategory[] = "events";
        constexpr int kLabelMax = 96; // stack buffer for a per callback guard label (no heap alloc)

        struct Subscription
        {
            const CubeApi* owner;
            uint32_t token;
            CubeEvent event;
            CubeEventFn fn;
            void* user;
        };

        OwnerRegistry<Subscription> g_registry;
        std::atomic<int> g_dispatchInFlight{0};

        // Live subscriber count per event, so a detour backed event can arm its backing on the zero to
        // one edge and drop it on the one to zero edge. Guarded by its own mutex: the registry's lock is
        // held while it erases, and eventbacking patches the game, which must not run under that lock.
        std::mutex g_countMutex;
        // Atomic so the per frame poller can read it lock free from the render thread; the mutex still
        // serializes the 0->1 / 1->0 edges that arm and release a backing.
        std::atomic<int32_t> g_eventSubs[CUBE_EVENT_COUNT] = {};

        bool validEvent(CubeEvent event)
        {
            return event >= 0 && event < CUBE_EVENT_COUNT;
        }

        // Bump the count for `event`; arms its backing when it becomes the first subscriber.
        void noteSubscribed(CubeEvent event)
        {
            bool first = false;
            {
                std::lock_guard<std::mutex> lock(g_countMutex);
                first = (g_eventSubs[event].fetch_add(1) == 0);
            }
            if (first)
                eventbacking::retainEvent(event);
        }

        // Drop the counts for a batch of removed subs; releases each backing whose last subscriber went.
        void noteUnsubscribed(const std::vector<CubeEvent>& removed)
        {
            for (const CubeEvent event : removed)
            {
                bool last = false;
                {
                    std::lock_guard<std::mutex> lock(g_countMutex);
                    if (g_eventSubs[event].load() > 0)
                        last = (g_eventSubs[event].fetch_sub(1) == 1);
                }
                if (last)
                    eventbacking::releaseEvent(event);
            }
        }

        // Per frame/per message events fire constantly; logging each delivery would flood the log.
        bool isHighFrequency(CubeEvent event)
        {
            switch (event)
            {
                case CUBE_EVENT_FRAME:
                case CUBE_EVENT_WNDPROC:
                case CUBE_EVENT_DEVICE_RESET:
                    return true;
                default:
                    return false;
            }
        }

    }

    const char* eventName(CubeEvent event)
    {
        switch (event)
        {
            case CUBE_EVENT_STARTUP:
                return "STARTUP";
            case CUBE_EVENT_SHUTDOWN:
                return "SHUTDOWN";
            case CUBE_EVENT_FRAME:
                return "FRAME";
            case CUBE_EVENT_DEVICE_RESET:
                return "DEVICE_RESET";
            case CUBE_EVENT_WNDPROC:
                return "WNDPROC";
            case CUBE_EVENT_PLAYER_ATTACK:
                return "PLAYER_ATTACK";
            case CUBE_EVENT_PLAYER_JUMP:
                return "PLAYER_JUMP";
            case CUBE_EVENT_AREA_CHANGE:
                return "AREA_CHANGE";
            case CUBE_EVENT_PLAYER_DAMAGED:
                return "PLAYER_DAMAGED";
            case CUBE_EVENT_CREATURE_DAMAGED:
                return "CREATURE_DAMAGED";
            case CUBE_EVENT_PLAYER_CRIT:
                return "PLAYER_CRIT";
            case CUBE_EVENT_MENU_OPEN:
                return "MENU_OPEN";
            case CUBE_EVENT_MENU_CLOSE:
                return "MENU_CLOSE";
            case CUBE_EVENT_PLAYER_LEVELUP:
                return "PLAYER_LEVELUP";
            case CUBE_EVENT_PLAYER_DEATH:
                return "PLAYER_DEATH";
            case CUBE_EVENT_PLAYER_RESPAWN:
                return "PLAYER_RESPAWN";
            case CUBE_EVENT_PLAYER_LAND:
                return "PLAYER_LAND";
            case CUBE_EVENT_MOVEMENT_CHANGED:
                return "MOVEMENT_CHANGED";
            case CUBE_EVENT_TARGET_CHANGED:
                return "TARGET_CHANGED";
            case CUBE_EVENT_CREATURE_SPAWN:
                return "CREATURE_SPAWN";
            case CUBE_EVENT_CREATURE_DEATH:
                return "CREATURE_DEATH";
            case CUBE_EVENT_COINS_CHANGED:
                return "COINS_CHANGED";
            case CUBE_EVENT_DAY_NIGHT:
                return "DAY_NIGHT";
            case CUBE_EVENT_BUFF_GAINED:
                return "BUFF_GAINED";
            case CUBE_EVENT_BUFF_LOST:
                return "BUFF_LOST";
            case CUBE_EVENT_EQUIPMENT_CHANGED:
                return "EQUIPMENT_CHANGED";
            case CUBE_EVENT_SKILL_RANK_CHANGED:
                return "SKILL_RANK_CHANGED";
            case CUBE_EVENT_AIM_TARGET_CHANGED:
                return "AIM_TARGET_CHANGED";
            case CUBE_EVENT_CREATURE_DESPAWN:
                return "CREATURE_DESPAWN";
            case CUBE_EVENT_COMPANION_SUMMONED:
                return "PET_SUMMONED";
            case CUBE_EVENT_COMPANION_DIED:
                return "PET_DIED";
            case CUBE_EVENT_COMPANION_DISMISSED:
                return "PET_DISMISSED";
            case CUBE_EVENT_PLAYER_STUNNED:
                return "PLAYER_STUNNED";
            case CUBE_EVENT_PLAYER_KNOCKED_DOWN:
                return "PLAYER_KNOCKED_DOWN";
            case CUBE_EVENT_PLAYER_RECOVERED:
                return "PLAYER_RECOVERED";
            case CUBE_EVENT_CREATURE_STUNNED:
                return "CREATURE_STUNNED";
            case CUBE_EVENT_CREATURE_KNOCKED_DOWN:
                return "CREATURE_KNOCKED_DOWN";
            case CUBE_EVENT_COMPANION_STUNNED:
                return "PET_STUNNED";
            case CUBE_EVENT_COMPANION_KNOCKED_DOWN:
                return "PET_KNOCKED_DOWN";
            case CUBE_EVENT_CREATURE_SELECTED:
                return "CREATURE_SELECTED";
            case CUBE_EVENT_ITEM_PICKUP:
                return "ITEM_PICKUP";
            case CUBE_EVENT_PLAYER_ROLL:
                return "PLAYER_ROLL";
            case CUBE_EVENT_CREATURE_RECOVERED:
                return "CREATURE_RECOVERED";
            case CUBE_EVENT_COMPANION_RECOVERED:
                return "PET_RECOVERED";
            case CUBE_EVENT_ABILITY_USED:
                return "ABILITY_USED";
            case CUBE_EVENT_READY:
                return "READY";
            case CUBE_EVENT_WORLD_ENTER:
                return "WORLD_ENTER";
            case CUBE_EVENT_WORLD_EXIT:
                return "WORLD_EXIT";
            case CUBE_EVENT_CHAT_MESSAGE:
                return "CHAT_MESSAGE";
            default:
                return "?";
        }
    }

    uint32_t subscribe(const CubeApi* owner, CubeEvent event, CubeEventFn fn, void* user)
    {
        if (!owner || !fn || !validEvent(event))
            return 0;

        const uint32_t token = g_registry.add(Subscription{owner, 0, event, fn, user});
        noteSubscribed(event);
        LOGC(Trace, kCategory, "'%s' subscribed %s (token %u, %zu total)", ownerName(owner), eventName(event),
             token, g_registry.size());

        return token;
    }

    int32_t unsubscribe(uint32_t token)
    {
        if (!token)
            return 0;

        std::vector<CubeEvent> removed;
        const std::size_t dropped = g_registry.removeToken(token, [&removed](const Subscription& sub)
                                                           { removed.push_back(sub.event); });
        noteUnsubscribed(removed);
        if (dropped)
            LOGC(Debug, kCategory, "unsubscribed token %u", token);

        return dropped ? 1 : 0;
    }

    bool hasSubscriber(CubeEvent event)
    {
        return validEvent(event) && g_eventSubs[event].load(std::memory_order_relaxed) > 0;
    }

    void unsubscribeOwner(const CubeApi* owner)
    {
        std::vector<CubeEvent> removed;
        const std::size_t dropped = g_registry.removeOwner(owner, [&removed](const Subscription& sub)
                                                           { removed.push_back(sub.event); });
        noteUnsubscribed(removed);

        if (dropped)
        {
            // The caller unmaps the mod's DLL as soon as we return.
            barrier::drain(g_dispatchInFlight, "event unsubscribe");
            LOGC(Debug, kCategory, "dropped %zu subscription(s) for an unloaded mod (drained)", dropped);
        }
    }

    int32_t emit(const CubeEventArgs& args)
    {
        // Before the snapshot, not just around the dispatch: the unmap window opens the moment
        // subscriptions are copied out of the registry.
        barrier::InFlight inflight(g_dispatchInFlight);

        // Reused per thread snapshot buffer so a hot event does not heap allocate each frame. A
        // synchronous re emit would clobber it, so an in use flag falls back to a fresh local vector.
        static thread_local std::vector<Subscription> shared;
        static thread_local bool sharedInUse = false;
        std::vector<Subscription> local;
        const bool useShared = !sharedInUse;
        std::vector<Subscription>& matched = useShared ? shared : local;
        sharedInUse = true;
        g_registry.snapshotInto(matched, [&](const Subscription& sub) { return sub.event == args.event; });

        // Dispatch low to high priority so the highest priority mod runs last (final say on swallow);
        // within one priority, a dependency's lower topological rank runs before its dependents. The
        // monotonic token breaks remaining ties, which makes this equivalent to a stable sort without
        // the per call temporary buffer stable_sort allocates.
        std::sort(matched.begin(), matched.end(),
                  [](const Subscription& a, const Subscription& b)
                  {
                      if (ownerPriority(a.owner) != ownerPriority(b.owner))
                          return ownerPriority(a.owner) < ownerPriority(b.owner);
                      if (ownerOrder(a.owner) != ownerOrder(b.owner))
                          return ownerOrder(a.owner) < ownerOrder(b.owner);
                      return a.token < b.token;
                  });

        if (!matched.empty() && !isHighFrequency(args.event))
            LOGC(Trace, kCategory, "%s -> %zu listener(s) (subject=0x%08X param=%d param2=%d amount=%.1f)",
                 eventName(args.event), matched.size(), args.subject, args.param, args.param2, args.amount);

        int32_t swallow = 0;
        for (const Subscription& sub : matched)
        {
            if (faultguard::isQuarantined(sub.owner))
                continue; // a mod disabled after a fault no longer receives events

            CubeEventArgs callArgs = args;
            callArgs.user = sub.user;
            callArgs.swallow = 0;
            // Stack label (no heap alloc) for attribution if the callback throws/faults.

            char label[kLabelMax];
            std::snprintf(label, sizeof(label), "mod '%s' %s callback", ownerName(sub.owner),
                          eventName(sub.event));
            guard::tryRun(label, sub.owner, [&]() { sub.fn(&callArgs); });

            if (callArgs.swallow)
            {
                swallow = 1;
                if (args.event == CUBE_EVENT_WNDPROC)
                    LOGC(Trace, kCategory, "WNDPROC msg 0x%04X swallowed by '%s'", args.msg,
                         ownerName(sub.owner));
            }
        }

        if (useShared)
            sharedInUse = false;

        return swallow;
    }

    void clear()
    {
        const std::size_t n = g_registry.size();

        if (n)
            LOGC(Trace, kCategory, "cleared %zu subscription(s)", n);

        g_registry.clear();
        {
            std::lock_guard<std::mutex> lock(g_countMutex);
            for (int32_t i = 0; i < CUBE_EVENT_COUNT; ++i)
                g_eventSubs[i].store(0);
        }
        // Every subscription is gone, so nothing can still want a detour armed on its behalf.
        eventbacking::releaseAll();
    }

    std::string describeOwner(const CubeApi* owner)
    {
        std::string out;
        g_registry.forEach(
            [&](const Subscription& sub)
            {
                if (sub.owner != owner)
                    return;

                if (!out.empty())
                    out += ", ";

                out += eventName(sub.event);
            });

        return out;
    }

    void forEachSubscription(const std::function<void(const CubeApi*, const char*)>& fn)
    {
        g_registry.forEach([&](const Subscription& sub) { fn(sub.owner, eventName(sub.event)); });
    }
}