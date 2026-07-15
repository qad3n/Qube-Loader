#pragma once
// Local player + combat accessors: Hero (alias Player), Combat, Stun.

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
        int getHitStun() const { return m_data.hitStun; } // stun-lock timer, 0..600
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

    // The local player. Snapshots loader data on construction; getters read the snapshot.
    class Hero
    {
    public:
        explicit Hero(const CubeApi* api) : m_api(api), m_valid(api && api->player.get(api, &m_data) != 0) {}

        bool valid() const { return m_valid; }
        // Re-pull the snapshot from live memory; call after a setter, else a get reads the pre-set value.
        bool refresh() { m_valid = m_api && m_api->player.get(m_api, &m_data) != 0; return m_valid; }
        bool reload() { return refresh(); }
        bool isAlive() const { return m_data.alive != 0; }
        float getHealth() const { return m_data.health; }
        int getLevel() const { return m_data.level; }
        unsigned getXp() const { return m_data.xp; }
        Class getClass() const { return static_cast<Class>(m_data.classForm); }
        const char* getClassName() const { return m_data.className; }
        int getSpec() const { return m_data.spec; }
        int getType() const { return m_data.type; }
        const char* getName() const { return m_data.hasName ? m_data.name : ""; }
        bool hasName() const { return m_data.hasName != 0; }
        Vec3 getPosition() const { return Vec3{m_data.x, m_data.y, m_data.z}; }
        bool hasPosition() const { return m_data.hasPosition != 0; }
        float getMana() const { return m_data.mana; }
        float getManaPercent() const { return m_data.manaPercent; }
        float getStamina() const { return m_data.stamina; }
        float getStaminaPercent() const { return m_data.staminaPercent; }
        int getCoins() const { return m_data.coins; }
        float getFacing() const { return m_data.facing; }
        Vec3 getVelocity() const { return Vec3{m_data.velX, m_data.velY, m_data.velZ}; }
        float getSpeed() const { return m_data.speed; }
        // Consolidated state (resolved by the loader) - prefer these over the booleans.
        Movement getMovement() const { return static_cast<Movement>(m_data.movement); }
        Action getAction() const { return static_cast<Action>(m_data.action); }
        const char* getMovementText() const { return movementName(getMovement()); }
        const char* getActionText() const { return actionName(getAction()); }
        // Legacy convenience: thin wrappers over the consolidated state (no recompute).
        bool isOnGround() const { return getMovement() == Movement::Grounded; }
        bool isClimbing() const { return getMovement() == Movement::Climbing; }
        bool isSwimming() const { return getMovement() == Movement::Swimming; }
        bool isAttacking() const { return getAction() == Action::Attacking; }
        bool isSneaking() const { return m_data.sneaking != 0; }
        // Stealth stat 0..1 (reduces enemy detection; shares the field that feeds crit chance).
        float getStealth() const { return m_data.stealth; }
        // Held lantern / light render flag (this is what the old mislabeled "sneak" toggle drove).
        bool hasLantern() const { return m_data.lantern != 0; }
        // ms elapsed in the current action; a small value with isAttacking() means an attack just began.
        int getActionElapsedMs() const { return m_data.actionElapsedMs; }
        bool isCasting() const { return getAction() == Action::Casting; }
        bool isKnockedDown() const { return m_data.knockedDown != 0; }
        // Consolidated stun snapshot (timer + downed + knockback), plus a one-call break-free.
        Stun getStun() const { return Stun(m_api, 0); } // 0 = the local player
        bool clearStun() const { return m_api && m_api->status.clearStun(m_api, 0) != 0; }
        int getActionId() const { return m_data.actionId; }
        bool hasState() const { return m_data.hasState != 0; }
        const CubePlayer& raw() const { return m_data; }

        // Mutators write live memory (guarded), not this frozen snapshot; call refresh() to see the change here.
        bool setHealth(float health) const { return setStat(PlayerStat::Health, health); }
        bool setMana(float mana) const { return setStat(PlayerStat::Mana, mana); }
        bool setStamina(float stamina) const { return setStat(PlayerStat::Stamina, stamina); }
        bool setCoins(int coins) const { return setStat(PlayerStat::Coins, coins); }
        bool setLevel(int level) const { return setStat(PlayerStat::Level, level); }
        bool giveXp(int amount) const { return m_api && m_api->player.addXp(m_api, amount) != 0; }
        bool teleport(float x, float y, float z) const { return m_api && m_api->player.teleport(m_api, x, y, z) != 0; }
        bool teleport(const Vec3& to) const { return teleport(to.x, to.y, to.z); }
        // Generic scalar setter plus typed conveniences for every editable stat.
        bool setStat(PlayerStat stat, double value) const { return m_api && m_api->player.setStat(m_api, static_cast<int32_t>(stat), value) != 0; }
        bool setClass(Class value) const { return setStat(PlayerStat::Class, static_cast<double>(static_cast<int>(value))); }
        bool setSpec(int spec) const { return setStat(PlayerStat::Spec, spec); }
        bool setType(int type) const { return setStat(PlayerStat::Type, type); }
        bool setFacing(float radians) const { return setStat(PlayerStat::Facing, radians); }
        bool setPower(float power) const { return setStat(PlayerStat::Power, power); }
        bool setArmor(float armor) const { return setStat(PlayerStat::Armor, armor); }
        bool setSpirit(float spirit) const { return setStat(PlayerStat::Spirit, spirit); }
        bool setCombo(int combo) const { return setStat(PlayerStat::Combo, combo); }
        bool setAttackCooldown(float cooldown) const { return setStat(PlayerStat::AttackCooldown, cooldown); }
        bool setHitStun(int hitStun) const { return setStat(PlayerStat::HitStun, hitStun); }
        bool setVelocity(float x, float y, float z) const { return setStat(PlayerStat::VelX, x) && setStat(PlayerStat::VelY, y) && setStat(PlayerStat::VelZ, z); }
        bool setVelocity(const Vec3& v) const { return setVelocity(v.x, v.y, v.z); }
        bool setActionId(int actionId) const { return setStat(PlayerStat::ActionId, actionId); }
        bool setSneaking(bool sneaking) const { return setStat(PlayerStat::Sneaking, sneaking ? 1 : 0); }
        // Set the stealth level directly (0..1); shares the crit field, so this also sets crit contribution.
        bool setStealth(float stealth) const { return setStat(PlayerStat::Crit, stealth); }
        // Turn the held lantern/light on or off.
        bool setLantern(bool on) const { return setStat(PlayerStat::Lantern, on ? 1 : 0); }
        bool setAttackSpeed(float attackSpeed) const { return setStat(PlayerStat::AttackSpeed, attackSpeed); }
        bool setCrit(float crit) const { return setStat(PlayerStat::Crit, crit); }
        bool setBaseDamage(float damage) const { return setStat(PlayerStat::BaseDamage, damage); }
        bool setName(const char* name) const { return m_api && m_api->player.setName(m_api, name) != 0; }
        bool setSkillRank(int index, int value) const { return m_api && m_api->skills.setRank(m_api, index, value) != 0; }

    private:
        const CubeApi* m_api;
        CubePlayer m_data = {};
        bool m_valid;
    };

    typedef Hero Player;

    // Local-player combat snapshot: stored stats plus loader-maintained damage/hit telemetry.
    class Combat
    {
    public:
        explicit Combat(const CubeApi* api) : m_api(api), m_valid(api && api->combat.get(api, &m_data) != 0) {}

        bool valid() const { return m_valid; }
        bool refresh() { m_valid = m_api && m_api->combat.get(m_api, &m_data) != 0; return m_valid; }
        bool reload() { return refresh(); }
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
        float getLastDamageTaken() const { return m_data.lastDamageTaken; }
        float getLastDamageDealt() const { return m_data.lastDamageDealt; }
        unsigned getHits() const { return m_data.hits; }
        unsigned getCrits() const { return m_data.crits; }
        unsigned getDamageTakenEvents() const { return m_data.damageTakenEvents; }
        const CubeCombat& raw() const { return m_data; }

    private:
        const CubeApi* m_api;
        CubeCombat m_data = {};
        bool m_valid;
    };

}
