#pragma once
// Locale: the mod-facing localization facade. Translates keys against this mod's own
// <dllDir>/lang/<stem>/<locale>.ini files (the loader keys them by the mod's DLL stem); every call is
// a guarded one-liner returning the fallback if the loader is unavailable. Get with mod.locale().

#include "cube/common.hpp"

#include <string>

namespace cube
{
    class Locale
    {
    public:
        explicit Locale(const CubeApi* api = nullptr) : m_api(api) {}

        // Translates key in the active locale. A missing key returns fallback, or the key itself when
        // fallback is null (so an untranslated string still renders something meaningful).
        std::string translate(const char* key, const char* fallback = nullptr) const
        {
            const char* effective = fallback ? fallback : key;
            if (!m_api)
                return effective ? effective : "";
            char buffer[CUBE_LOCALE_STRING_MAX];
            m_api->locale.translate(m_api, key, fallback, buffer, sizeof(buffer));
            return std::string(buffer);
        }

        bool setLocale(const char* locale) const
        {
            return m_api && m_api->locale.setLocale(m_api, locale) != 0;
        }

        std::string getLocale() const
        {
            if (!m_api)
                return "";
            char buffer[CUBE_LOCALE_STRING_MAX];
            m_api->locale.getLocale(m_api, buffer, sizeof(buffer));
            return std::string(buffer);
        }

    private:
        const CubeApi* m_api;
    };

}
