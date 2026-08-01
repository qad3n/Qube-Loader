#pragma once
// Pickup accessor: the last item the local player picked up (E / hold to pickup). The E-key twin of
// cube::Selection - both are detour-backed captures of the player's most recent interaction.

#include "cube/items.hpp"

namespace cube
{
    class Pickup
    {
    public:
        explicit Pickup(const CubeApi* api)
            : m_api(api), m_valid(api && api->pickup.getLast(api, &m_data) != 0)
        {
        }

        bool valid() const { return m_valid; } // false until the first pickup happens (arms on first call)
        bool refresh()
        {
            m_valid = m_api && m_api->pickup.getLast(m_api, &m_data) != 0;
            return m_valid;
        }
        // The picked item (a transient staging copy: raw().address is 0, so no live edit).
        Item item() const { return Item(m_data, m_api); }
        int getStack() const { return m_data.stack; } // how many were picked up
        const CubeItem& raw() const { return m_data; }

    private:
        const CubeApi* m_api;
        CubeItem m_data = {};
        bool m_valid;
    };
}
