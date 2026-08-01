#pragma once
// Local player combat-stats snapshot: stored Creature combat stats (all direct reads). Mirrors the
// game's combat_* subsystem (the functions that read these stats). Note the enemy-AI class
// cube::CombatBehavior is a different thing and is not wrapped here. For damage and crit occurrences
// use the PLAYER_DAMAGED / PLAYER_CRIT events or the IMPACT / CRIT_ROLL hooks.

#include "cube/common.hpp"

namespace cube
{
    class Combat
    {
    public:
        explicit Combat(const CubeApi* api) : m_api(api), m_valid(api && api->combat.get(api, &m_data) != 0)
        {
        }

        bool valid() const { return m_valid; }
        bool refresh()
        {
            m_valid = m_api && m_api->combat.get(m_api, &m_data) != 0;
            return m_valid;
        }
        float getBaseDamage() const { return m_data.baseDamage; }
        float getPower() const { return m_data.power; }
        float getArmor() const { return m_data.armor; }
        float getSpirit() const { return m_data.spirit; }
        int getCombo() const { return m_data.combo; }
        float getAttackCooldown() const { return m_data.attackCooldown; }
        bool isReadyToStrike() const { return m_data.attackCooldown <= 0.0f; }
        float getAttackSpeed() const { return m_data.attackSpeed; }
        float getCritStat() const { return m_data.critStat; }
        float getCritChancePercent() const { return m_data.critChancePercent; }
        int getHitStun() const { return m_data.hitStun; }
        bool isStunned() const { return m_data.hitStun > 0; }
        const CubeCombat& raw() const { return m_data; }

    private:
        const CubeApi* m_api;
        CubeCombat m_data = {};
        bool m_valid;
    };
}
