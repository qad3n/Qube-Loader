#include "features/locale_demo.h"
#include "mod_context.h"

namespace exmod
{
    LocaleDemo& localeDemo()
    {
        static LocaleDemo g_demo;
        return g_demo;
    }

    void LocaleDemo::install(cube::Mod& mod)
    {
        m_mod = &mod;
        // Demonstrate translate at load: greet in the active locale, falling back to the given text when
        // the lang file or key is missing (so this still logs something on a fresh install).
        cubeLogf(g_api, CUBE_LOG_INFO, "example_mod: locale '%s' greeting -> \"%s\"",
                 mod.locale().getLocale().c_str(),
                 mod.locale().translate("greeting", "Hello, adventurer!").c_str());
    }

    void LocaleDemo::setLocale(const char* locale)
    {
        if (m_mod)
            m_mod->locale().setLocale(locale);
    }

    std::string LocaleDemo::locale() const
    {
        return m_mod ? m_mod->locale().getLocale() : std::string();
    }

    std::string LocaleDemo::translate(const char* key) const
    {
        return m_mod ? m_mod->locale().translate(key) : std::string(key ? key : "");
    }
}
