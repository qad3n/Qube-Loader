#include "modloader/core/events.h"
#include "modloader/core/owner_name.h"
#include "modloader/core/registry.h"
#include "core/log.h"
#include "core/faultguard.h"
#include "util/guard.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace modloader::events
{
    namespace
    {

        constexpr char kCategory[] = "events";
        constexpr int kLabelMax = 96; // stack buffer for a per-callback guard label (no heap alloc)

        struct Subscription
        {
            const CubeApi* owner;
            uint32_t token;
            CubeEvent event;
            CubeEventFn fn;
            void* user;
        };

        OwnerRegistry<Subscription> g_registry;

        // Per-frame/per-message events fire constantly; logging each delivery would flood the log.
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
            case CUBE_EVENT_ENTITY_DAMAGED:
                return "ENTITY_DAMAGED";
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
            case CUBE_EVENT_ENTITY_SPAWN:
                return "ENTITY_SPAWN";
            case CUBE_EVENT_ENTITY_DEATH:
                return "ENTITY_DEATH";
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
            case CUBE_EVENT_ENTITY_DESPAWN:
                return "ENTITY_DESPAWN";
            case CUBE_EVENT_PET_SUMMONED:
                return "PET_SUMMONED";
            case CUBE_EVENT_PET_DIED:
                return "PET_DIED";
            case CUBE_EVENT_PET_DISMISSED:
                return "PET_DISMISSED";
            case CUBE_EVENT_PLAYER_STUNNED:
                return "PLAYER_STUNNED";
            case CUBE_EVENT_PLAYER_KNOCKED_DOWN:
                return "PLAYER_KNOCKED_DOWN";
            case CUBE_EVENT_PLAYER_RECOVERED:
                return "PLAYER_RECOVERED";
            case CUBE_EVENT_ENTITY_STUNNED:
                return "ENTITY_STUNNED";
            case CUBE_EVENT_ENTITY_KNOCKED_DOWN:
                return "ENTITY_KNOCKED_DOWN";
            case CUBE_EVENT_PET_STUNNED:
                return "PET_STUNNED";
            case CUBE_EVENT_PET_KNOCKED_DOWN:
                return "PET_KNOCKED_DOWN";
            case CUBE_EVENT_ENTITY_SELECTED:
                return "ENTITY_SELECTED";
            case CUBE_EVENT_ITEM_PICKUP:
                return "ITEM_PICKUP";
            case CUBE_EVENT_ENTITY_RECOVERED:
                return "ENTITY_RECOVERED";
            case CUBE_EVENT_PET_RECOVERED:
                return "PET_RECOVERED";
            case CUBE_EVENT_ABILITY_USED:
                return "ABILITY_USED";
            default:
                return "?";
        }
    }

    uint32_t subscribe(const CubeApi* owner, CubeEvent event, CubeEventFn fn, void* user)
    {
        if (!owner || !fn || event < 0 || event >= CUBE_EVENT_COUNT)
            return 0;

        const uint32_t token = g_registry.add(Subscription{owner, 0, event, fn, user});
        LOGC(Trace, kCategory, "'%s' subscribed %s (token %u, %zu total)", ownerName(owner), eventName(event), token, g_registry.size());

        return token;
    }

    int32_t unsubscribe(uint32_t token)
    {
        if (!token)
            return 0;

        const std::size_t dropped = g_registry.removeToken(token);
        if (dropped)
            LOGC(Debug, kCategory, "unsubscribed token %u", token);

        return dropped ? 1 : 0;
    }

    void unsubscribeOwner(const CubeApi* owner)
    {
        const std::size_t dropped = g_registry.removeOwner(owner);

        if (dropped)
            LOGC(Debug, kCategory, "dropped %zu subscription(s) for an unloaded mod", dropped);
    }

    int32_t emit(const CubeEventArgs& args)
    {
        // Reused per-thread snapshot buffer so a hot event does not heap-allocate each frame. A
        // synchronous re-emit would clobber it, so an in-use flag falls back to a fresh local vector.
        static thread_local std::vector<Subscription> shared;
        static thread_local bool sharedInUse = false;
        std::vector<Subscription> local;
        const bool useShared = !sharedInUse;
        std::vector<Subscription>& matched = useShared ? shared : local;
        sharedInUse = true;
        g_registry.snapshotInto(matched, [&](const Subscription& sub) { return sub.event == args.event; });

        // Dispatch low-to-high priority so the highest-priority mod runs last (final say on swallow);
        // stable_sort keeps load order within one priority.
        std::stable_sort(matched.begin(), matched.end(), [](const Subscription& a, const Subscription& b)
        {
            return ownerPriority(a.owner) < ownerPriority(b.owner);
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
            std::snprintf(label, sizeof(label), "mod '%s' %s callback", ownerName(sub.owner), eventName(sub.event));
            guard::tryRun(label, sub.owner, [&]()
            {
                sub.fn(&callArgs);
            });

            if (callArgs.swallow)
            {
                swallow = 1;
                if (args.event == CUBE_EVENT_WNDPROC)
                    LOGC(Trace, kCategory, "WNDPROC msg 0x%04X swallowed by '%s'", args.msg, ownerName(sub.owner));
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
    }

    std::string describeOwner(const CubeApi* owner)
    {
        std::string out;
        g_registry.forEach([&](const Subscription& sub)
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
        g_registry.forEach([&](const Subscription& sub)
        {
            fn(sub.owner, eventName(sub.event));
        });
    }
}