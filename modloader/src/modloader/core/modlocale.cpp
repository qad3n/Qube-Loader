#include "modloader/core/modlocale.h"
#include "core/ini.h"
#include "core/log.h"
#include "core/paths.h"

#include <cstdlib>
#include <map>

namespace modloader::modlocale
{
    namespace
    {
        constexpr char kCategory[] = "modloc";
        constexpr char kDirName[] = "lang";
        constexpr char kFileSuffix[] = ".ini";
        constexpr char kDefaultLocale[] = "en";
        constexpr char kLocaleEnvVar[] = "CUBE_MOD_LOCALE";

        struct LocaleFile
        {
            ini::KeyValues values;
            bool loaded = false; // cache populated from disk (an absent file loads as a clean empty map)
        };

        struct ModLang
        {
            std::string locale; // active locale (defaults to g_defaultLocale on first touch)
            std::map<std::string, LocaleFile> files; // keyed by sanitized locale
        };

        std::string g_dir; // <dllDir>/lang
        std::string g_defaultLocale = kDefaultLocale;
        std::map<std::string, ModLang> g_mods; // keyed by sanitized DLL stem

        std::string pathOf(const std::string& safeStem, const std::string& safeLocale)
        {
            const std::string modDir = paths::join(g_dir, safeStem.c_str());
            return paths::join(modDir, (safeLocale + kFileSuffix).c_str());
        }

        // The mod's state, defaulting its active locale to the loader default on first touch.
        ModLang& modOf(const std::string& modStem)
        {
            const std::string safeStem = paths::sanitizeComponent(modStem);
            ModLang& mod = g_mods[safeStem];
            if (mod.locale.empty())
                mod.locale = g_defaultLocale;
            return mod;
        }

        // The mod's cached file for its active locale, loaded on first touch. Empty g_dir -> empty map.
        LocaleFile& fileOf(ModLang& mod, const std::string& modStem)
        {
            const std::string safeStem = paths::sanitizeComponent(modStem);
            const std::string safeLocale = paths::sanitizeComponent(mod.locale);
            LocaleFile& file = mod.files[safeLocale];
            if (!file.loaded)
            {
                if (!g_dir.empty())
                    file.values = ini::read(pathOf(safeStem, safeLocale));
                file.loaded = true;
            }
            return file;
        }
    }

    void init(const std::string& dllDir)
    {
        g_dir = paths::join(dllDir, kDirName);
        g_mods.clear();
        const char* env = std::getenv(kLocaleEnvVar);
        g_defaultLocale = (env && env[0]) ? env : kDefaultLocale;
        if (!paths::ensureDir(g_dir))
            LOGC(Warn, kCategory, "could not create lang dir %s; mod translations will fall back to keys", g_dir.c_str());
    }

    std::string translate(const std::string& modStem, const std::string& key, const std::string& fallback)
    {
        ModLang& mod = modOf(modStem);
        LocaleFile& file = fileOf(mod, modStem);
        const ini::KeyValues::const_iterator it = file.values.find(ini::lower(ini::trim(key)));
        return it == file.values.end() ? fallback : it->second;
    }

    void setLocale(const std::string& modStem, const std::string& locale)
    {
        modOf(modStem).locale = locale.empty() ? g_defaultLocale : locale;
    }

    std::string getLocale(const std::string& modStem)
    {
        return modOf(modStem).locale;
    }
}
