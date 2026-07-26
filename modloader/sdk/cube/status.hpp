#pragma once
// Status wrapper: Stun (hit-stun / knockdown snapshot). Mirrors the game's status handling on the
// Creature. Buff nodes are read through the Creature / Player surface (creature.hpp).

#include "cube/common.hpp"

namespace cube
{
    class Stun
    {
    public:
        Stun() = default;
        Stun(const CubeApi* api, unsigned address) : m_api(api), m_valid(api && api->status.stun(api, address, &m_data) != 0) {}

        bool valid() const { return m_valid; }
        bool isStunned() const { return m_data.stunned != 0; } // cannot act while true
        bool isKnockedDown() const { return m_data.knockedDown != 0; } // on the ground, "stars"
        int getHitStun() const { return m_data.hitStun; } // stun lock timer, 0..600
        float getHitStunPercent() const { return m_data.hitStunPercent; } // 0..100 for a bar
        Vec3 getKnockback() const { return Vec3{m_data.knockbackX, m_data.knockbackY, 0.0f}; }
        unsigned getAddress() const { return m_data.address; }
        const CubeStun& raw() const { return m_data; }
        // Break the stun (zero the timer + stand up if downed). Returns true on success.
        bool clear() const { return m_valid && m_api && m_api->status.clearStun(m_api, m_data.address) != 0; }

    private:
        const CubeApi* m_api = nullptr;
        CubeStun m_data = {};
        bool m_valid = false;
    };
}
