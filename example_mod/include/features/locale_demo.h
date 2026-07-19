#pragma once
// Showcases the per-mod localization API (ABI 23): translate keys against
// lang/example_mod/<locale>.ini, switch the active locale live, and fall back to the key when a
// translation is missing. The menu (Mod tab) views the translations and drives the locale switch.
#include "cube_mod.hpp"

#include <string>

namespace exmod
{
    class LocaleDemo
    {
    public:
        void install(cube::Mod& mod);
        void setLocale(const char* locale);
        std::string locale() const;
        std::string translate(const char* key) const;

    private:
        cube::Mod* m_mod = nullptr;
    };

    LocaleDemo& localeDemo();
}
