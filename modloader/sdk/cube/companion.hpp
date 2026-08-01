#pragma once
// Active companion accessor: Companion.

#include "cube/common.hpp"

namespace cube
{
    class Companion
    {
    public:
        explicit Companion(const CubeApi* api)
            : m_api(api), m_valid(api && api->companion.get(api, &m_data) != 0)
        {
        }

        bool valid() const { return m_valid; }
        bool refresh()
        {
            m_valid = m_api && m_api->companion.get(m_api, &m_data) != 0;
            return m_valid;
        }
        const char* getName() const { return m_data.hasName ? m_data.name : ""; }
        int getType() const { return m_data.type; }
        int getLevel() const { return m_data.level; }
        unsigned getXp() const { return m_data.xp; }
        float getHealth() const { return m_data.health; }
        Vec3 getPosition() const { return Vec3{m_data.x, m_data.y, m_data.z}; }
        Vec3 getVelocity() const { return Vec3{m_data.velX, m_data.velY, m_data.velZ}; }
        float getFacing() const { return m_data.facing; }
        CreatureState getState() const { return static_cast<CreatureState>(m_data.creatureState); }
        bool isAlive() const { return getState() == CreatureState::Alive; }
        const CubeCompanion& raw() const { return m_data; }
        // Live edits (the pet is a Creature; edits go through the creature setters).
        bool set(PlayerStat stat, double value) const
        {
            return m_api && m_data.address &&
                   m_api->creatures.setStat(m_api, m_data.address, static_cast<int32_t>(stat), value) != 0;
        }
        bool setHealth(float health) const { return set(PlayerStat::Health, health); }
        bool setLevel(int level) const { return set(PlayerStat::Level, level); }
        bool setType(int type) const { return set(PlayerStat::Type, type); }
        bool setFacing(float radians) const { return set(PlayerStat::Facing, radians); }
        bool setVelocity(float x, float y, float z) const
        {
            return set(PlayerStat::VelX, x) && set(PlayerStat::VelY, y) && set(PlayerStat::VelZ, z);
        }
        bool setName(const char* name) const
        {
            return m_api && m_data.address && m_api->creatures.setName(m_api, m_data.address, name) != 0;
        }
        bool teleport(float x, float y, float z) const
        {
            return m_api && m_data.address && m_api->creatures.teleport(m_api, m_data.address, x, y, z) != 0;
        }
        bool teleport(const Vec3& to) const { return teleport(to.x, to.y, to.z); }

    private:
        const CubeApi* m_api;
        CubeCompanion m_data = {};
        bool m_valid;
    };

}
