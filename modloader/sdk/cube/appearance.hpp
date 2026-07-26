#pragma once
// Appearance accessor: a creature's stored appearance record (client only, read only). Writing
// appearance persists to the character save and is intentionally not exposed.

#include "cube/common.hpp"

namespace cube
{
    // Value type over an appearance snapshot. Construct via Appearance(api) for the local player, or
    // Appearance(api, creatureAddress) for any creature.
    class Appearance
    {
    public:
        explicit Appearance(const CubeApi* api) : m_api(api), m_valid(api && api->appearance.get(api, &m_data) != 0) {}
        Appearance(const CubeApi* api, unsigned creatureAddress)
            : m_api(api), m_valid(api && api->appearance.of(api, creatureAddress, &m_data) != 0) {}

        bool valid() const { return m_valid; }
        bool refresh() { m_valid = m_api && m_api->appearance.get(m_api, &m_data) != 0; return m_valid; }
        unsigned getAddress() const { return m_data.address; } // the resolved Creature (0 if unavailable)
        int getSpecies() const { return m_data.species; } // race/species selector (drives the model)
        bool isFemale() const { return m_data.gender != 0; }
        int getStyle() const { return m_data.style; }
        int getColor0() const { return m_data.color0; }
        int getColor1() const { return m_data.color1; }
        int getColor2() const { return m_data.color2; }
        const CubeAppearance& raw() const { return m_data; }

    private:
        const CubeApi* m_api;
        CubeAppearance m_data = {};
        bool m_valid;
    };
}
