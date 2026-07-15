#include "modloader/core/internal.h"
#include "modloader/core/events.h"
#include "modloader/core/conflict.h"
#include "modloader/core/owner_name.h"
#include "game/gamehooks/gamehooks.h"
#include "core/log.h"
#include "cube_sdk.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

// Load-time compatibility report: log what every mod attached to and warn about anything they share.
namespace modloader
{
    namespace
    {
        // Append owner to index[key], de-duplicated, for the shared-hook/event inverse index.
        void addOwner(std::map<std::string, std::vector<std::string>>& index, const std::string& key, const std::string& owner)
        {
            std::vector<std::string>& owners = index[key];
            for (const std::string& existing : owners)
            {
                if (existing == owner)
                    return;
            }

            owners.push_back(owner);
        }

        std::string joinNames(const std::vector<std::string>& names)
        {
            std::string out;
            for (const std::string& name : names)
            {
                if (!out.empty())
                    out += ", ";
                out += "'";
                out += name;
                out += "'";
            }

            return out;
        }

    }

    // Intercept hooks shared by >1 mod are loud (last-writer-wins); shared observe events are informational.
    void reportCompatibility()
    {
        LOGC(Debug, kCategory, "compatibility report: %zu mod(s) loaded", loadedMods().size());
        for (const std::unique_ptr<LoadedMod>& mod : loadedMods())
        {
            const std::string events = modloader::events::describeOwner(&mod->context.api);
            const std::string hooks = game::gamehooks::describeOwner(&mod->context.api);
            LOGC(Debug, kCategory, "  %s (priority %d): events=[%s] hooks=[%s]", mod->context.category.c_str(),
                 mod->context.priority, events.empty() ? "-" : events.c_str(), hooks.empty() ? "-" : hooks.c_str());
        }

        std::map<std::string, std::vector<std::string>> hookOwners;
        game::gamehooks::forEachSubscription([&](const CubeApi* owner, const char* label)
        {
            addOwner(hookOwners, label, ownerName(owner));
        });
        for (const std::pair<const std::string, std::vector<std::string>>& entry : hookOwners)
        {
            if (entry.second.size() > 1)
                conflict::warn("hook %s is intercepted by %s; they run last-writer-wins on the return and may conflict",
                               entry.first.c_str(), joinNames(entry.second).c_str());
        }

        std::map<std::string, std::vector<std::string>> eventOwners;
        modloader::events::forEachSubscription([&](const CubeApi* owner, const char* label)
        {
            addOwner(eventOwners, label, ownerName(owner));
        });
        for (const std::pair<const std::string, std::vector<std::string>>& entry : eventOwners)
        {
            if (entry.second.size() > 1)
                LOGC(Debug, kCategory, "event %s observed by %s (independent observers, no conflict)",
                     entry.first.c_str(), joinNames(entry.second).c_str());
        }
    }

}
