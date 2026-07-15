#include "modloader/core/modregistry.h"
#include "core/ini.h"
#include "core/log.h"
#include "core/paths.h"

#include <cstdlib>
#include <map>

namespace modloader::modregistry
{
    namespace
    {
        constexpr char kCategory[] = "modreg";
        constexpr char kFileName[] = "mods.ini";
        constexpr char kEnabledSuffix[] = ".enabled";
        constexpr char kStrikesSuffix[] = ".strikes";
        constexpr int32_t kFaultStrikeLimit = 3;

        struct Entry
        {
            bool enabled = true;
            int32_t strikes = 0;
        };

        std::map<std::string, Entry> g_entries;
        std::string g_dir;

        bool endsWith(const std::string& s, const char* suffix)
        {
            const size_t n = std::string(suffix).size();
            return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
        }

        int32_t parseInt(const std::string& s)
        {
            return static_cast<int32_t>(std::strtol(s.c_str(), nullptr, 10));
        }

        void persist()
        {
            if (g_dir.empty())
                return;
            ini::KeyValues kv;
            for (const std::map<std::string, Entry>::value_type& e : g_entries)
            {
                kv[e.first + kEnabledSuffix] = e.second.enabled ? "1" : "0";
                kv[e.first + kStrikesSuffix] = std::to_string(e.second.strikes);
            }
            if (!ini::write(paths::join(g_dir, kFileName), kv))
                LOGC(Warn, kCategory, "could not write %s", kFileName);
        }
    }

    void load(const std::string& dllDir)
    {
        g_dir = dllDir;
        g_entries.clear();
        const ini::KeyValues kv = ini::read(paths::join(dllDir, kFileName));
        for (const ini::KeyValues::value_type& e : kv)
        {
            if (endsWith(e.first, kEnabledSuffix))
            {
                const std::string stem = e.first.substr(0, e.first.size() - (sizeof(kEnabledSuffix) - 1));
                bool enabled = true;
                ini::parseBool(e.second, enabled);
                g_entries[stem].enabled = enabled;
            }
            else if (endsWith(e.first, kStrikesSuffix))
            {
                const std::string stem = e.first.substr(0, e.first.size() - (sizeof(kStrikesSuffix) - 1));
                g_entries[stem].strikes = parseInt(e.second);
            }
        }
    }

    bool isEnabled(const std::string& stem)
    {
        const std::map<std::string, Entry>::const_iterator it = g_entries.find(ini::lower(stem));
        return it == g_entries.end() ? true : it->second.enabled;
    }

    void noteSeen(const std::string& stem)
    {
        const std::string key = ini::lower(stem);
        if (g_entries.find(key) == g_entries.end())
        {
            g_entries[key] = Entry();
            persist();
        }
    }

    void setEnabled(const std::string& stem, bool enabled)
    {
        const std::string key = ini::lower(stem);
        Entry& entry = g_entries[key];
        if (entry.enabled != enabled)
        {
            entry.enabled = enabled;
            persist();
        }
    }

    int32_t recordFault(const std::string& stem)
    {
        const std::string key = ini::lower(stem);
        Entry& entry = g_entries[key];
        ++entry.strikes;
        if (entry.strikes >= kFaultStrikeLimit && entry.enabled)
        {
            entry.enabled = false;
            LOGC(Warn, kCategory, "mod '%s' disabled after %d faults; re-enable it in %s to retry",
                 key.c_str(), entry.strikes, kFileName);
        }
        persist();
        return entry.strikes;
    }
}
