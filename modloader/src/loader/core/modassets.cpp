#include "loader/core/modassets.h"
#include "loader/core/conflict.h"
#include "game/assets.h"
#include "core/log.h"

#include <map>
#include <mutex>
#include <set>
#include <string>

namespace modloader::modassets
{
    namespace
    {
        constexpr char kCategory[] = "modasset";

        // Mods register from any callback thread while teardown drops owners on the mod thread.
        std::mutex g_mutex;
        std::map<std::string, std::set<std::string>> g_byStem; // stem to keys it owns
        std::map<std::string, std::string> g_owner; // key to current owning stem
    }

    bool registerAsset(const std::string& modStem, const std::string& key, const void* data, int32_t size)
    {
        if (modStem.empty() || key.empty())
            return false;
        if (!game::assets::available())
            return false;
        if (!game::assets::setOverride(key, data, size))
            return false;

        std::string displaced;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            const std::map<std::string, std::string>::iterator owned = g_owner.find(key);
            if (owned != g_owner.end() && owned->second != modStem)
            {
                displaced = owned->second;
                g_byStem[displaced].erase(key);
            }

            g_owner[key] = modStem;
            g_byStem[modStem].insert(key);
        }

        if (!displaced.empty())
            conflict::warn("asset '%s' from '%s' now replaces '%s' (last writer wins)", key.c_str(),
                           modStem.c_str(), displaced.c_str());
        LOGC(Debug, kCategory, "'%s' registered asset '%s' (%d bytes)", modStem.c_str(), key.c_str(), size);
        return true;
    }

    bool unregisterAsset(const std::string& modStem, const std::string& key)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
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
        std::lock_guard<std::mutex> lock(g_mutex);
        const std::map<std::string, std::string>::const_iterator owned = g_owner.find(key);
        return owned != g_owner.end() && owned->second == modStem;
    }

    void dropOwner(const std::string& modStem)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
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
