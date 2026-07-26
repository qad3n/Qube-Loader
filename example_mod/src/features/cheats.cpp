#include "features/cheats.h"
#include "mod_context.h"

#include "cube_mod.hpp"

namespace exmod
{
    Cheats& cheats()
    {
        static Cheats g_cheats;
        return g_cheats;
    }

    void Cheats::apply() const
    {
        if (!g_api)
            return;

        cube::Player player(g_api);

        if (!player.valid())
            return;

        if (m_settings.godMode)
            player.setHealth(m_settings.godModeHealth);

        if (m_settings.infiniteMana)
            player.setMana(kFullResource);

        if (m_settings.infiniteStamina)
            player.setStamina(kFullResource);
    }
}
