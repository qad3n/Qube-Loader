#pragma once
// Entity accessors + collection helpers (entities/equipment/inventory/skills/buffs).

#include "cube/common.hpp"
#include "cube/items.hpp"

namespace cube
{
    class Entity
    {
    public:
        Entity() = default;
        explicit Entity(const CubeEntity& data, const CubeApi* api = nullptr) : m_data(data), m_api(api) {}

        const char* getName() const { return m_data.hasName ? m_data.name : ""; }
        int getCategory() const { return m_data.category; }
        int getType() const { return m_data.type; }
        int getLevel() const { return m_data.level; }
        float getHealth() const { return m_data.health; }
        Vec3 getPosition() const { return Vec3{m_data.x, m_data.y, m_data.z}; }
        Vec3 getVelocity() const { return Vec3{m_data.velX, m_data.velY, m_data.velZ}; }
        float getFacing() const { return m_data.facing; }
        float getDistance() const { return m_data.distance; }
        Relation getRelation() const { return static_cast<Relation>(m_data.relation); }
        EntityState getState() const { return static_cast<EntityState>(m_data.entityState); }
        bool isBoss() const { return m_data.boss != 0; }
        bool isElite() const { return m_data.elite != 0; } // derived: boss or high star rank
        int getRank() const { return m_data.rank; } // star / power rank (the monster power tier)
        int getCombatClass() const { return m_data.combatClass; } // 1 Warrior/2 Ranger/3 Mage/4 Rogue
        int getEffectivePower() const { return m_data.effectivePower; } // compare vs your own level
        int getHitStun() const { return m_data.hitStun; }
        bool isStaggered() const { return m_data.hitStun > 0; }
        bool isKnockedDown() const { return m_data.knockedDown != 0; }
        // Consolidated stun snapshot for this creature, plus a one-call break-free.
        Stun getStun() const { return Stun(m_api, m_data.address); }
        bool clearStun() const { return m_api && m_data.address && m_api->status.clearStun(m_api, m_data.address) != 0; }
        unsigned getOwnerAddress() const { return m_data.ownerAddress; }
        bool isAlive() const { return getState() == EntityState::Alive; }
        bool isHostile() const { return m_data.hostile != 0; } // game's foe rule: kind==monster OR aggroed
        Item getWeapon() const { return Item(m_data.weapon, m_api); }
        unsigned getAddress() const { return m_data.address; }
        const CubeEntity& raw() const { return m_data; }
        // True if this is a tameable passive critter (feed it a Food item to tame it as a pet).
        bool isTameable() const { return m_api && m_data.address && m_api->entities.isTameable(m_api, m_data.address) != 0; }
        // This creature's inventory / stock (e.g. a shopkeeper's wares). Empty if it carries none.
        std::vector<Item> stock() const;
        // This creature's active status effects (the status list is generic per creature).
        std::vector<class Buff> buffs() const;
        // Live edits to this creature (validated as a Creature by the loader).
        bool setStat(PlayerStat stat, double value) const { return m_api && m_data.address && m_api->entities.setStat(m_api, m_data.address, static_cast<int32_t>(stat), value) != 0; }
        bool setHealth(float health) const { return setStat(PlayerStat::Health, health); }
        bool setLevel(int level) const { return setStat(PlayerStat::Level, level); }
        bool setType(int type) const { return setStat(PlayerStat::Type, type); }
        bool setFacing(float radians) const { return setStat(PlayerStat::Facing, radians); }
        bool setVelocity(float x, float y, float z) const { return setStat(PlayerStat::VelX, x) && setStat(PlayerStat::VelY, y) && setStat(PlayerStat::VelZ, z); }
        bool setCategory(int category) const { return setStat(PlayerStat::Category, category); }
        bool setRank(int rank) const { return setStat(PlayerStat::Rank, rank); }
        bool setName(const char* name) const { return m_api && m_data.address && m_api->entities.setName(m_api, m_data.address, name) != 0; }
        bool teleport(float x, float y, float z) const { return m_api && m_data.address && m_api->entities.teleport(m_api, m_data.address, x, y, z) != 0; }
        bool teleport(const Vec3& to) const { return teleport(to.x, to.y, to.z); }

    private:
        CubeEntity m_data = {};
        const CubeApi* m_api = nullptr;
    };

    // Nearby entities, usable with any CubeApi* (what Mod::entities() calls). Tree walk is in the loader.
    inline std::vector<Entity> entitiesOf(const CubeApi* api)
    {
        std::vector<Entity> result;
        if (!api)
            return result;
        // Heap buffer: CUBE_ENTITIES_MAX CubeEntity on the stack would risk a stack overflow.
        std::vector<CubeEntity> buffer(CUBE_ENTITIES_MAX);
        const int32_t count = api->entities.list(api, buffer.data(), CUBE_ENTITIES_MAX);
        result.reserve(static_cast<size_t>(count));
        for (int32_t i = 0; i < count; ++i)
            result.push_back(Entity(buffer[static_cast<size_t>(i)], api));
        return result;
    }

    inline bool targetOf(const CubeApi* api, Entity& out)
    {
        if (!api)
            return false;
        CubeEntity data = {};
        data.structSize = sizeof(CubeEntity);
        if (api->entities.target(api, &data) == 0)
            return false;
        out = Entity(data, api);
        return true;
    }

    // The crosshair aim/hover target, distinct from the committed selection returned by targetOf.
    inline bool aimTargetOf(const CubeApi* api, Entity& out)
    {
        if (!api)
            return false;
        CubeEntity data = {};
        data.structSize = sizeof(CubeEntity);
        if (api->entities.aimTarget(api, &data) == 0)
            return false;
        out = Entity(data, api);
        return true;
    }

    inline std::vector<Item> equipmentOf(const CubeApi* api)
    {
        return detail::fillList<CubeItem, Item, CUBE_EQUIP_SLOTS>(api, api ? api->items.equipment : nullptr);
    }

    inline std::vector<Item> inventoryOf(const CubeApi* api)
    {
        return detail::fillList<CubeItem, Item, CUBE_INVENTORY_MAX>(api, api ? api->items.inventory : nullptr);
    }

    // The inventory / stock of any creature by address (e.g. a merchant's wares).
    inline std::vector<Item> stockOf(const CubeApi* api, unsigned creatureAddress)
    {
        std::vector<Item> result;
        if (!api || !creatureAddress)
            return result;
        CubeItem buffer[CUBE_INVENTORY_MAX];
        const int32_t count = api->items.inventoryOf(api, creatureAddress, buffer, CUBE_INVENTORY_MAX);
        result.reserve(static_cast<size_t>(count));
        for (int32_t i = 0; i < count; ++i)
            result.push_back(Item(buffer[i], api));
        return result;
    }

    inline std::vector<Item> Entity::stock() const { return stockOf(m_api, m_data.address); }

    // Resolve any (type, subtype) to its display name via the full item directory.
    inline const char* itemName(const CubeApi* api, int type, int subtype)
    {
        return (api && api->items.name) ? api->items.name(api, type, subtype) : "";
    }
    inline const char* itemName(const CubeApi* api, ItemType type, int subtype)
    {
        return itemName(api, static_cast<int>(type), subtype);
    }

    inline std::vector<int> skillsOf(const CubeApi* api)
    {
        std::vector<int> result;
        if (!api)
            return result;
        int32_t buffer[CUBE_SKILL_COUNT];
        const int32_t count = api->skills.ranks(api, buffer, CUBE_SKILL_COUNT);
        result.assign(buffer, buffer + count);
        return result;
    }

    // A live ability cooldown timer. Base durations are code formulas (not stored); only the
    // remaining timer is editable (setRemaining(0) = ready).
    class AbilityCooldown
    {
    public:
        AbilityCooldown() = default;
        explicit AbilityCooldown(const CubeAbilityCooldown& data, const CubeApi* api = nullptr) : m_data(data), m_api(api) {}

        int getAbilityId() const { return m_data.abilityId; }
        int getRemainingMs() const { return m_data.remainingMs; }
        bool isReady() const { return m_data.remainingMs <= 0; }
        const CubeAbilityCooldown& raw() const { return m_data; }
        bool setRemaining(int ms) const { return m_api && m_api->skills.setCooldown(m_api, m_data.abilityId, ms) != 0; }
        bool ready() const { return setRemaining(0); }

    private:
        CubeAbilityCooldown m_data = {};
        const CubeApi* m_api = nullptr;
    };

    inline std::vector<AbilityCooldown> abilityCooldownsOf(const CubeApi* api)
    {
        return detail::fillList<CubeAbilityCooldown, AbilityCooldown, CUBE_ABILITIES_MAX>(api, api ? api->skills.cooldowns : nullptr);
    }

    // An active status effect / buff / debuff on the local player.
    class Buff
    {
    public:
        Buff() = default;
        explicit Buff(const CubeBuff& data, const CubeApi* api = nullptr) : m_data(data), m_api(api) {}

        int getType() const { return m_data.type; }
        float getMagnitude() const { return m_data.magnitude; }
        int getRemainingMs() const { return m_data.remainingMs; }
        const CubeBuff& raw() const { return m_data; }
        // Live edits to this effect node.
        bool setField(BuffField field, double value) const { return m_api && m_data.address && m_api->status.setField(m_api, m_data.address, static_cast<int32_t>(field), value) != 0; }
        bool setType(int type) const { return setField(BuffField::Type, type); }
        bool setMagnitude(float magnitude) const { return setField(BuffField::Magnitude, magnitude); }
        bool setRemainingMs(int ms) const { return setField(BuffField::Duration, ms); }

    private:
        CubeBuff m_data = {};
        const CubeApi* m_api = nullptr;
    };

    inline std::vector<Buff> buffsOf(const CubeApi* api)
    {
        return detail::fillList<CubeBuff, Buff, CUBE_BUFFS_MAX>(api, api ? api->status.effects : nullptr);
    }

    // An entity's own status effects, read through the generic per-creature status list.
    inline std::vector<Buff> Entity::buffs() const
    {
        std::vector<Buff> result;
        if (!m_api || !m_data.address)
            return result;
        CubeBuff buffer[CUBE_BUFFS_MAX];
        const int32_t count = m_api->entities.effects(m_api, m_data.address, buffer, CUBE_BUFFS_MAX);
        result.reserve(static_cast<size_t>(count));
        for (int32_t i = 0; i < count; ++i)
            result.push_back(Buff(buffer[i], m_api));
        return result;
    }
}
