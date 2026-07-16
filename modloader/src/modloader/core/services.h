#pragma once
// Inter-mod ecosystem services: a named/versioned shared-service registry plus directed messaging
// addressed by a mod's manifest id. Both are owner-tagged (OwnerRegistry) so a mod's registrations and
// message handlers drop automatically on unload. The service impl pointer and message payloads cross
// the DLL boundary raw - the loader never dereferences them; provider and consumer own their layout.
#include "cube_sdk.h"

#include <cstdint>

namespace modloader::services
{
    // Publish impl under name at version (>=1) for owner; re-registering the same name from the same
    // owner replaces it. Returns false on bad args.
    bool registerService(const CubeApi* owner, const char* name, uint32_t version, void* impl);
    // Withdraw owner's service named name. Returns true if one existed.
    bool unregisterService(const CubeApi* owner, const char* name);
    // Highest-version provider of name with version >= minVersion, else nullptr. Writes the chosen
    // version to outVersion when non-null.
    void* query(const char* name, uint32_t minVersion, uint32_t* outVersion);

    // Register owner's message receiver; returns a nonzero token (0 on bad args).
    uint32_t addMessageHandler(const CubeApi* owner, CubeMessageFn fn, void* user);
    // Remove a receiver by its token for owner. Returns true if it existed.
    bool removeMessageHandler(const CubeApi* owner, uint32_t token);
    // Deliver msgId + payload to the mod whose manifest id == targetModId; returns the receiving
    // handler's CubeMessageArgs.result, or -1 if the target id is unknown or has no message handler.
    int32_t sendMessage(const CubeApi* sender, const char* targetModId, uint32_t msgId, void* payload, uint32_t payloadSize);

    // Drop all of a mod's services + message handlers (on unload). clear() empties both registries.
    void unregisterOwner(const CubeApi* owner);
    void clear();
}
