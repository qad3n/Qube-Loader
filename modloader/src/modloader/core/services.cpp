#include "modloader/core/services.h"
#include "modloader/core/internal.h"
#include "modloader/core/registry.h"
#include "modloader/core/owner_name.h"
#include "core/faultguard.h"
#include "core/log.h"
#include "util/guard.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace modloader::services
{
    namespace
    {
        constexpr char kCategory[] = "services";
        constexpr int32_t kNoTarget = -1;
        constexpr int kLabelMax = 128;

        struct Service
        {
            const CubeApi* owner;
            uint32_t token = 0; // stamped by OwnerRegistry::add
            std::string name;
            uint32_t version;
            void* impl;
        };

        struct MessageSub
        {
            const CubeApi* owner;
            uint32_t token = 0; // stamped by OwnerRegistry::add
            CubeMessageFn fn;
            void* user;
        };

        OwnerRegistry<Service> g_services;
        OwnerRegistry<MessageSub> g_messages;

        // Resolve a target mod's CubeApi from its manifest id (nullptr if no loaded mod owns that id).
        const CubeApi* resolveTarget(const char* targetModId)
        {
            for (const std::unique_ptr<LoadedMod>& mod : loadedMods())
            {
                if (mod->context.id == targetModId)
                    return &mod->context.api;
            }
            return nullptr;
        }
    }

    bool registerService(const CubeApi* owner, const char* name, uint32_t version, void* impl)
    {
        if (!owner || !name || !name[0] || version == 0 || !impl)
            return false;

        // Replace an existing same-name service from this owner so a re-register updates in place.
        const std::string wanted = name;
        g_services.removeIf([owner, &wanted](const Service& s) { return s.owner == owner && s.name == wanted; });

        Service service;
        service.owner = owner;
        service.name = wanted;
        service.version = version;
        service.impl = impl;
        g_services.add(service);
        LOGC(Debug, kCategory, "'%s' registered service '%s' v%u", ownerId(owner), name, version);
        return true;
    }

    bool unregisterService(const CubeApi* owner, const char* name)
    {
        if (!owner || !name)
            return false;
        const std::string wanted = name;
        const std::size_t removed = g_services.removeIf([owner, &wanted](const Service& s)
        {
            return s.owner == owner && s.name == wanted;
        });
        if (removed)
            LOGC(Debug, kCategory, "'%s' unregistered service '%s'", ownerId(owner), name);
        return removed > 0;
    }

    void* query(const char* name, uint32_t minVersion, uint32_t* outVersion)
    {
        if (!name)
            return nullptr;

        const std::string wanted = name;
        void* best = nullptr;
        uint32_t bestVersion = 0;
        g_services.forEach([&](const Service& s)
        {
            if (s.name != wanted || s.version < minVersion)
                return;
            if (!best || s.version > bestVersion)
            {
                best = s.impl;
                bestVersion = s.version;
            }
        });

        if (best && outVersion)
            *outVersion = bestVersion;
        return best;
    }

    uint32_t addMessageHandler(const CubeApi* owner, CubeMessageFn fn, void* user)
    {
        if (!owner || !fn)
            return 0;
        MessageSub sub;
        sub.owner = owner;
        sub.fn = fn;
        sub.user = user;
        const uint32_t token = g_messages.add(sub);
        LOGC(Debug, kCategory, "'%s' registered a message handler (token %u)", ownerId(owner), token);
        return token;
    }

    bool removeMessageHandler(const CubeApi* owner, uint32_t token)
    {
        if (!owner || !token)
            return false;
        const std::size_t removed = g_messages.removeIf([owner, token](const MessageSub& s)
        {
            return s.owner == owner && s.token == token;
        });
        return removed > 0;
    }

    int32_t sendMessage(const CubeApi* sender, const char* targetModId, uint32_t msgId, void* payload, uint32_t payloadSize)
    {
        if (!targetModId)
            return kNoTarget;

        const CubeApi* target = resolveTarget(targetModId);
        if (!target)
        {
            LOGC(Debug, kCategory, "'%s' sendMessage to unknown mod id '%s'", ownerId(sender), targetModId);
            return kNoTarget;
        }

        std::vector<MessageSub> handlers;
        g_messages.snapshotInto(handlers, [target](const MessageSub& s) { return s.owner == target; });
        if (handlers.empty())
            return kNoTarget;

        const char* senderId = ownerId(sender);
        int32_t result = kNoTarget;
        for (const MessageSub& sub : handlers)
        {
            if (faultguard::isQuarantined(sub.owner))
                continue;

            CubeMessageArgs args = {};
            args.structSize = sizeof(CubeMessageArgs);
            args.senderId = senderId;
            args.msgId = msgId;
            args.payload = payload;
            args.payloadSize = payloadSize;
            args.result = kNoTarget;

            char label[kLabelMax];
            std::snprintf(label, sizeof(label), "mod '%s' message handler", ownerName(sub.owner));
            guard::tryRun(label, sub.owner, [&]()
            {
                sub.fn(sub.owner, &args, sub.user);
            });
            result = args.result;
        }
        return result;
    }

    void unregisterOwner(const CubeApi* owner)
    {
        const std::size_t services = g_services.removeOwner(owner);
        const std::size_t messages = g_messages.removeOwner(owner);
        if (services || messages)
            LOGC(Trace, kCategory, "dropped %zu service(s) + %zu message handler(s) for an unloaded mod",
                 services, messages);
    }

    void clear()
    {
        g_services.clear();
        g_messages.clear();
    }
}
