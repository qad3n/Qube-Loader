#include "modloader/core/modassets.h"
#include "modloader/core/conflict.h"
#include "game/assets.h"
#include "core/log.h"

#include <map>
#include <set>
#include <string>

namespace modloader::modassets
{
    namespace
    {
        constexpr char kCategory[] = "modasset";

        std::map<std::string, std::set<std::string>> g_byStem; // stem to keys it owns
        std::map<std::string, std::string> g_owner;            // key to current owning stem
    }

    bool registerAsset(const std::string& modStem, const std::string& key, const void* data, int32_t size)
    {
        if (modStem.empty() || key.empty())
            return false;
        if (!game::assets::available())
            return false;
        if (!game::assets::setOverride(key, data, size))
            return false;

        const std::map<std::string, std::string>::iterator owned = g_owner.find(key);
        if (owned != g_owner.end() && owned->second != modStem)
        {
            conflict::warn("asset '%s' from '%s' now replaces '%s' (last writer wins)",
                           key.c_str(), modStem.c_str(), owned->second.c_str());
            g_byStem[owned->second].erase(key);
        }

        g_owner[key] = modStem;
        g_byStem[modStem].insert(key);
        LOGC(Debug, kCategory, "'%s' registered asset '%s' (%d bytes)", modStem.c_str(), key.c_str(), size);
        return true;
    }

    bool unregisterAsset(const std::string& modStem, const std::string& key)
    {
        const std::map<std::string, std::string>::iterator owned = g_owner.find(key);
        if (owned == g_owner.end() || owned->second != modStem)
            return false;

        game::assets::removeOverride(key);
        g_owner.erase(owned);
        g_byStem[modStem].erase(key);
        return true;
    }

    bool hasAsset(const std::string& modStem, const std::string& key)
    {
        const std::map<std::string, std::string>::const_iterator owned = g_owner.find(key);
        return owned != g_owner.end() && owned->second == modStem;
    }

    void dropOwner(const std::string& modStem)
    {
        const std::map<std::string, std::set<std::string>>::iterator it = g_byStem.find(modStem);
        if (it == g_byStem.end())
            return;

        for (const std::string& key : it->second)
        {
            const std::map<std::string, std::string>::iterator owned = g_owner.find(key);
            if (owned != g_owner.end() && owned->second == modStem)
            {
                game::assets::removeOverride(key);
                g_owner.erase(owned);
            }
        }
        g_byStem.erase(it);
    }
}
