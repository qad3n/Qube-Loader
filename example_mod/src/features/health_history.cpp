#include "features/health_history.h"
#include "mod_context.h"

#include "cube_mod.hpp"

namespace exmod
{
    HealthHistory& healthHistory()
    {
        static HealthHistory g_healthHistory;
        return g_healthHistory;
    }

    void HealthHistory::sample()
    {
        if (!g_api)
            return;
        cube::Hero hero(g_api);
        if (!hero.valid())
            return;
        m_health[m_head] = hero.getHealth();
        m_head = (m_head + 1) % kSize;
    }
}
