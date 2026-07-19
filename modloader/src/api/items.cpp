#include "api/bridge.h"
#include "core/log.h"
#include "game/items.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiItemsEquipment(const CubeApi* api, CubeItem* out, int32_t maxCount)
        {
            return bridgeList(api, "items.equipment", out, maxCount, game::listEquipment(out, maxCount), &CubeItem::address);
        }

        int32_t CUBE_CALL apiItemsInventory(const CubeApi* api, CubeItem* out, int32_t maxCount)
        {
            return bridgeList(api, "items.inventory", out, maxCount, game::listInventory(out, maxCount), &CubeItem::address);
        }

        int32_t CUBE_CALL apiItemsSetField(const CubeApi* api, uint32_t itemAddress, int32_t field, int32_t value)
        {
            if (!capabilityGate(api, CUBE_CAP_WRITES, "items.setField"))
                return 0;
            writeguard::Scope scope(api);
            const bool ok = game::setItemField(itemAddress, field, value);
            LOGC(Debug, kApiCategory, "'%s' items.setField(0x%08X, %d, %d) -> %s",
                 modName(api), itemAddress, field, value, ok ? "ok" : "fail");
            return okInt(ok);
        }

        const char* CUBE_CALL apiItemsName(const CubeApi* api, int32_t type, int32_t subtype)
        {
            static_cast<void>(api);
            return game::itemDisplayName(type, subtype);
        }

        int32_t CUBE_CALL apiItemsInventoryOf(const CubeApi* api, uint32_t creatureAddress, CubeItem* out, int32_t maxCount)
        {
            return bridgeList(api, "items.inventoryOf", out, maxCount, game::listInventoryOf(creatureAddress, out, maxCount), &CubeItem::address);
        }

        int32_t CUBE_CALL apiItemsValue(const CubeApi* api, uint32_t itemAddress)
        {
            const int32_t value = game::itemValue(itemAddress);
            LOGC(Trace, kApiCategory, "'%s' items.value(0x%08X) -> %d", modName(api), itemAddress, value);
            return value;
        }

        int32_t CUBE_CALL apiSkillsRanks(const CubeApi* api, int32_t* out, int32_t maxCount)
        {
            return bridgeList(api, "skills.ranks", maxCount, game::listSkillRanks(out, maxCount));
        }

        int32_t CUBE_CALL apiSkillsSetRank(const CubeApi* api, int32_t index, int32_t value)
        {
            return bridgeSetPair(api, "skills.setRank", &game::setSkillRank, index, value);
        }

        int32_t CUBE_CALL apiSkillsCooldowns(const CubeApi* api, CubeAbilityCooldown* out, int32_t maxCount)
        {
            return bridgeList(api, "skills.cooldowns", maxCount, game::listAbilityCooldowns(out, maxCount));
        }

        int32_t CUBE_CALL apiSkillsSetCooldown(const CubeApi* api, int32_t abilityId, int32_t remainingMs)
        {
            return bridgeSetPair(api, "skills.setCooldown", &game::setAbilityCooldown, abilityId, remainingMs);
        }

        int32_t CUBE_CALL apiSkillsClearCooldowns(const CubeApi* api)
        {
            if (!capabilityGate(api, CUBE_CAP_WRITES, "skills.clearCooldowns"))
                return 0;
            writeguard::Scope scope(api);
            const int32_t cleared = game::clearAbilityCooldowns();
            LOGC(Debug, kApiCategory, "'%s' skills.clearCooldowns -> %d", modName(api), cleared);
            return cleared;
        }

    }

    void fillItems(CubeApi& api)
    {
        api.items.equipment = &apiItemsEquipment;
        api.items.inventory = &apiItemsInventory;
        api.items.setField = &apiItemsSetField;
        api.items.name = &apiItemsName;
        api.items.inventoryOf = &apiItemsInventoryOf;
        api.items.value = &apiItemsValue;
        api.skills.ranks = &apiSkillsRanks;
        api.skills.setRank = &apiSkillsSetRank;
        api.skills.cooldowns = &apiSkillsCooldowns;
        api.skills.setCooldown = &apiSkillsSetCooldown;
        api.skills.clearCooldowns = &apiSkillsClearCooldowns;
    }

}
