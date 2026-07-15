#include "features/cheats.h"
#include "mod_context.h"

#include "cube_mod.hpp"

namespace exmod
{
    namespace
    {
        constexpr float kFullResource = 1.0f;
    }

    Cheats& cheats()
    {
        static Cheats g_cheats;
        return g_cheats;
    }

    void Cheats::apply() const
    {
        if (!g_api)
            return;

        cube::Hero hero(g_api);

        if (!hero.valid())
            return;

        if (m_settings.godMode)
            hero.setHealth(m_settings.godModeHealth);

        if (m_settings.infiniteMana)
            hero.setMana(kFullResource);

        if (m_settings.infiniteStamina)
            hero.setStamina(kFullResource);
    }
}
