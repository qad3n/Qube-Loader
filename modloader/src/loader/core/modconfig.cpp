#include "modloader/core/modconfig.h"
#include "core/ini.h"
#include "core/log.h"
#include "core/paths.h"

#include <cstdlib>
#include <map>

namespace modloader::modconfig
{
    namespace
    {
        constexpr char kCategory[] = "modcfg";
        constexpr char kDirName[] = "config";
        constexpr char kFileSuffix[] = ".ini";

        struct File
        {
            ini::KeyValues values;
            bool loaded = false; // cache populated from disk (an absent file loads as a clean empty map)
        };

        std::string g_dir; // <dllDir>/config
        std::map<std::string, File> g_files; // keyed by sanitized DLL stem

        std::string pathOf(const std::string& safeStem)
        {
            return paths::join(g_dir, (safeStem + kFileSuffix).c_str());
        }

        // The mod's cached file, loaded from disk on first touch. Empty g_dir (init not run) gives empty.
        File& fileOf(const std::string& modStem)
        {
            const std::string safeStem = paths::sanitizeComponent(modStem);
            File& file = g_files[safeStem];
            if (!file.loaded)
            {
                if (!g_dir.empty())
                    file.values = ini::read(pathOf(safeStem));
                file.loaded = true;
            }
            return file;
        }

        // Look up a key (case insensitive, matching ini::read's lowered keys); returns null if absent.
        const std::string* find(const std::string& modStem, const std::string& key)
        {
            const File& file = fileOf(modStem);
            const ini::KeyValues::const_iterator it = file.values.find(ini::lower(ini::trim(key)));
            return it == file.values.end() ? nullptr : &it->second;
        }

        bool store(const std::string& modStem, const std::string& key, const std::string& value)
        {
            if (g_dir.empty())
                return false;
            const std::string safeStem = paths::sanitizeComponent(modStem);
            File& file = fileOf(modStem);
            file.values[ini::lower(ini::trim(key))] = value;
            if (!ini::write(pathOf(safeStem), file.values))
            {
                LOGC(Warn, kCategory, "could not write config for '%s'", modStem.c_str());
                return false;
            }
            return true;
        }
    }

    void init(const std::string& dllDir)
    {
        g_dir = paths::join(dllDir, kDirName);
        g_files.clear();
        if (!paths::ensureDir(g_dir))
            LOGC(Warn, kCategory, "could not create config dir %s; mod settings will not persist", g_dir.c_str());
    }

    int32_t getInt(const std::string& modStem, const std::string& key, int32_t fallback)
    {
        const std::string* value = find(modStem, key);
        if (!value)
            return fallback;
        char* end = nullptr;
        const long parsed = std::strtol(value->c_str(), &end, 10);
        return end == value->c_str() ? fallback : static_cast<int32_t>(parsed);
    }

    double getFloat(const std::string& modStem, const std::string& key, double fallback)
    {
        const std::string* value = find(modStem, key);
        if (!value)
            return fallback;
        char* end = nullptr;
        const double parsed = std::strtod(value->c_str(), &end);
        return end == value->c_str() ? fallback : parsed;
    }

    bool getBool(const std::string& modStem, const std::string& key, bool fallback)
    {
        const std::string* value = find(modStem, key);
        bool out = fallback;
        if (value)
            ini::parseBool(*value, out);
        return out;
    }

    std::string getString(const std::string& modStem, const std::string& key, const std::string& fallback)
    {
        const std::string* value = find(modStem, key);
        return value ? *value : fallback;
    }

    bool setInt(const std::string& modStem, const std::string& key, int32_t value)
    {
        return store(modStem, key, std::to_string(value));
    }

    bool setFloat(const std::string& modStem, const std::string& key, double value)
    {
        return store(modStem, key, std::to_string(value));
    }

    bool setBool(const std::string& modStem, const std::string& key, bool value)
    {
        return store(modStem, key, value ? "1" : "0");
    }

    bool setString(const std::string& modStem, const std::string& key, const std::string& value)
    {
        return store(modStem, key, value);
    }
}
