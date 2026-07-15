#pragma once
// Event bus between the host and loaded mods. Each subscription is tagged with its owning CubeApi
// so all of a mod's subscriptions drop when it unloads.

#include "cube_sdk.h"
#include <functional>
#include <string>

namespace modloader::events
{
    // Registers fn and returns a nonzero subscription token (0 on bad args) to pass to unsubscribe.
    uint32_t subscribe(const CubeApi* owner, CubeEvent event, CubeEventFn fn, void* user);
    // Drops one subscription by its token; returns 1 if a subscription was removed, else 0.
    int32_t unsubscribe(uint32_t token);
    void unsubscribeOwner(const CubeApi* owner);
    // Dispatches to every subscriber of args.event; returns 1 if any callback set args.swallow.
    int32_t emit(const CubeEventArgs& args);
    void clear();
    // Human-readable event name for logs (shared with the api bridge).
    const char* eventName(CubeEvent event);
    // Comma-joined names of the events a mod is subscribed to (empty if none), for load-time logging.
    std::string describeOwner(const CubeApi* owner);
    // Enumerate all subscriptions as (owner, event-name) pairs; feeds the compatibility report's index.
    void forEachSubscription(const std::function<void(const CubeApi*, const char*)>& fn);
}