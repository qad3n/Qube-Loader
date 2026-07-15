#include "game/items.h"
#include "game/creature.h"
#include "game/gamecontroller.h"
#include "game/offsets.h"
#include "util/field.h"
#include "game/catalog.h"
#include "core/mem.h"
#include "util/guard.h"

#include <cstdint>
#include <cstdio>

namespace game
{
    namespace
    {
        int32_t clampInt(int32_t value, int32_t lo, int32_t hi)
        {
            if (value < lo)
                return lo;
            if (value > hi)
                return hi;
            return value;
        }

        // A catalog name exists iff the id is a real, renderable game id; unknown ids produce
        // the "?"/crash item.
        bool isKnownCatalogId(int32_t catalog, int32_t id)
        {
            return catalogName(catalog, id) != nullptr;
        }

        bool weaponSubtypeKnown(int32_t subtype)
        {
            return isKnownCatalogId(CUBE_CATALOG_WEAPON_SUBTYPE, subtype);
        }

        // Weapons MUST use a registered weapon subtype (unregistered ones render a blank name).
        // Non-weapon subtypes are not fully cataloged and are not a crash vector, so accept any.
        bool subtypeAcceptable(int32_t type, int32_t subtype)
        {
            if (type == off::kItemTypeWeapon)
                return weaponSubtypeKnown(subtype);
            return true;
        }

        bool isTwoHandedWeapon(int32_t subtype)
        {
            for (int32_t twoHanded : off::kTwoHandedWeaponSubtypes)
            {
                if (twoHanded == subtype)
                    return true;
            }
            return false;
        }

        int32_t maxUpgradesFor(int32_t type, int32_t subtype)
        {
            if (type == off::kItemTypeWeapon && isTwoHandedWeapon(subtype))
                return off::kItemUpgradeMaxTwoHand;
            return off::kItemUpgradeMaxOneHand;
        }

        // Why the item at itemBase is corrupt (renders "?" and can crash on draw), or nullptr
        // if valid. type is decisive: an unknown type (0 included) has no model/name/placement.
        const char* itemDefectReason(uintptr_t itemBase)
        {
            uint8_t type = 0;
            if (!mem::read(itemBase + off::kItemTypeOff, type))
                return "unreadable item memory";
            if (!isKnownCatalogId(CUBE_CATALOG_ITEM_TYPE, static_cast<int32_t>(type)))
                return "unknown item type (no model/name; welds to cursor; crashes on draw)";

            uint8_t subtype = 0;
            mem::read(itemBase + off::kItemSubtypeOff, subtype);
            // Only weapon subtypes are cataloged; an unregistered one renders blank. Non-weapon
            // subtypes are not a crash vector, so not flagged.
            if (type == off::kItemTypeWeapon && !weaponSubtypeKnown(static_cast<int32_t>(subtype)))
                return "unregistered weapon subtype (renders as '?')";
            return nullptr;
        }

        ItemDefect makeDefect(uintptr_t itemBase, ItemDefectLocation location, int32_t slot, const char* reason)
        {
            uint8_t type = 0;
            uint8_t subtype = 0;
            mem::read(itemBase + off::kItemTypeOff, type);
            mem::read(itemBase + off::kItemSubtypeOff, subtype);
            ItemDefect defect = {};
            defect.address = static_cast<uint32_t>(itemBase);
            defect.location = location;
            defect.slot = slot;
            defect.type = static_cast<int32_t>(type);
            defect.subtype = static_cast<int32_t>(subtype);
            defect.reason = reason;
            return defect;
        }

        const char* itemTypeName(int32_t type)
        {
            switch (type)
            {
                case off::kItemTypeEmpty:
                    return "empty";
                case off::kItemTypeWeapon:
                    return "weapon";
                case off::kItemTypeSpecial:
                    return "special";
                case off::kItemTypeCoins:
                    return "coins";
                case off::kItemTypeCurrency:
                    return "currency";
                default:
                    return "item";
            }
        }

        constexpr char kFoodDefaultName[] = "Bait"; // every un-named Food subtype is "Bait" in-game
        constexpr char kItemFallbackName[] = "item";
        constexpr char kEmptyItemName[] = "empty";

        // The per-type catalog that names an item's subtype, or -1 if the type has no subtype
        // variants (a singleton item whose name is the type name itself).
        int32_t subtypeCatalogFor(int32_t type)
        {
            switch (type)
            {
                case off::kItemTypeConsumable: return CUBE_CATALOG_CONSUMABLE_SUBTYPE;
                case off::kItemTypeWeapon: return CUBE_CATALOG_WEAPON_SUBTYPE;
                case off::kItemTypeSpecial: return CUBE_CATALOG_SPECIAL_SUBTYPE;
                case off::kItemTypeAccessory: return CUBE_CATALOG_ACCESSORY_SUBTYPE;
                case off::kItemTypeVehicle: return CUBE_CATALOG_VEHICLE_SUBTYPE;
                default: return -1;
            }
        }
    }

    // The full item directory: resolves (type, subtype) to the game's display name via the catalogs
    // extracted from the item-name registry. Food falls back to "Bait", singleton types to the type
    // name, and anything unmapped to "item". Never null.
    const char* itemDisplayName(int32_t type, int32_t subtype)
    {
        if (type == off::kItemTypeEmpty)
            return kEmptyItemName;
        if (type == off::kItemTypeFood)
        {
            const char* named = catalogName(CUBE_CATALOG_FOOD_SUBTYPE, subtype);
            return named ? named : kFoodDefaultName;
        }
        const int32_t subCatalog = subtypeCatalogFor(type);
        if (subCatalog >= 0)
        {
            const char* named = catalogName(subCatalog, subtype);
            if (named)
                return named;
        }
        const char* typeName = catalogName(CUBE_CATALOG_ITEM_TYPE, type);
        return typeName ? typeName : kItemFallbackName;
    }

    bool readItem(uintptr_t itemBase, CubeItem& out)
    {
        uint8_t type = 0;
        if (!mem::read(itemBase + off::kItemTypeOff, type))
            return false;

        uint8_t subtype = 0;
        uint8_t material = 0;
        uint8_t modifier = 0;
        int16_t level = 0;
        uint32_t seed = 0;
        int32_t upgrades = 0;

        mem::read(itemBase + off::kItemSubtypeOff, subtype);
        mem::read(itemBase + off::kItemMaterialOff, material);
        mem::read(itemBase + off::kItemModifierOff, modifier);
        mem::read(itemBase + off::kItemLevelOff, level);
        mem::read(itemBase + off::kItemSeedOff, seed);
        mem::read(itemBase + off::kItemUpgradeCountOff, upgrades);

        out.type = static_cast<int32_t>(type);
        out.subtype = static_cast<int32_t>(subtype);
        out.material = static_cast<int32_t>(material);
        out.modifier = static_cast<int32_t>(modifier);
        out.level = static_cast<int32_t>(level);
        out.seed = seed;
        out.upgradeCount = upgrades;

        std::snprintf(out.typeName, sizeof(out.typeName), "%s", itemTypeName(out.type));
        std::snprintf(out.name, sizeof(out.name), "%s", itemDisplayName(out.type, out.subtype));
        return type != 0;
    }

    bool readEquipmentSlot(uintptr_t creature, int32_t slot, CubeItem& out)
    {
        if (slot < 0 || slot >= off::kEquipmentSlotCount)
            return false;

        const uintptr_t itemBase = creature + off::kEquipmentBaseOff + static_cast<uintptr_t>(slot) * off::kEquipmentStride;
        out.structSize = sizeof(CubeItem);

        if (!readItem(itemBase, out))
            return false;

        out.address = static_cast<uint32_t>(itemBase);
        out.present = 1;
        out.slot = slot;
        out.stack = 1;
        return true;
    }

    int32_t listEquipment(CubeItem* out, int32_t maxCount)
    {
        if (!out || maxCount <= 0)
            return 0;

        uintptr_t gc = 0;
        uintptr_t player = 0;
        if (!resolveLocalPlayer(gc, player))
            return 0;

        int32_t count = 0;
        for (int32_t slot = 0; slot < off::kEquipmentSlotCount && count < maxCount; ++slot)
        {
            CubeItem item = {};
            if (readEquipmentSlot(player, slot, item))
                out[count++] = item;
        }
        return count;
    }

    namespace
    {
        // Walks a creature's inventory (vector<vector<cell>>; cell = [stack][Item 0x118]) into out[].
        // Shared by the local player's inventory and any creature's stock (a merchant's items).
        int32_t walkInventory(uintptr_t creature, CubeItem* out, int32_t maxCount)
        {
            uint32_t outerBegin = 0;
            int32_t outerCount = 0;
            if (!field::vectorSpan(creature + off::kInventoryVecOff, off::kInventoryOuterStride, off::kInventoryMaxOuter, outerBegin, outerCount))
                return 0;

            int32_t count = 0;
            for (int32_t o = 0; o < outerCount && count < maxCount; ++o)
            {
                const uintptr_t innerVec = outerBegin + static_cast<uintptr_t>(o) * off::kInventoryOuterStride;
                uint32_t innerBegin = 0;
                int32_t innerCount = 0;
                if (!field::vectorSpan(innerVec, off::kInventoryCellStride, off::kInventoryMaxInner, innerBegin, innerCount))
                    continue;

                for (int32_t j = 0; j < innerCount && count < maxCount; ++j)
                {
                    const uintptr_t cell = innerBegin + static_cast<uintptr_t>(j) * off::kInventoryCellStride;
                    int32_t stack = 0;
                    if (!mem::read(cell, stack) || stack <= 0)
                        continue;

                    CubeItem item = {};
                    item.structSize = sizeof(CubeItem);
                    if (!readItem(cell + off::kInventoryCellItemOff, item))
                        continue;

                    item.address = static_cast<uint32_t>(cell + off::kInventoryCellItemOff);
                    item.present = 1;
                    item.slot = -1;
                    item.stack = stack;
                    out[count++] = item;
                }
            }
            return count;
        }
    }

    int32_t listInventory(CubeItem* out, int32_t maxCount)
    {
        if (!out || maxCount <= 0)
            return 0;

        uintptr_t gc = 0;
        uintptr_t player = 0;
        if (!resolveLocalPlayer(gc, player))
            return 0;

        return walkInventory(player, out, maxCount);
    }

    int32_t listInventoryOf(uint32_t creatureAddress, CubeItem* out, int32_t maxCount)
    {
        if (!out || maxCount <= 0)
            return 0;

        uintptr_t obj = 0;
        if (!validateCreature(creatureAddress, obj))
            return 0;

        return walkInventory(obj, out, maxCount);
    }

    int32_t itemValue(uint32_t itemAddress)
    {
        const uintptr_t addr = static_cast<uintptr_t>(itemAddress);
        if (!addr || !mem::readable(reinterpret_cast<const void*>(addr), off::kItemStructSize))
            return 0;

        // Game-call the price function (int __thiscall(Item*)); __fastcall shim on mingw.
        typedef int32_t(__fastcall* ItemValueFn)(void* self, void* edx);
        const ItemValueFn fn = reinterpret_cast<ItemValueFn>(mem::rebase(off::kItemValueFn));
        int32_t value = 0;
        guard::tryRun("item value", [&]() { value = fn(reinterpret_cast<void*>(addr), nullptr); });
        return value;
    }

    int32_t listSkillRanks(int32_t* out, int32_t maxCount)
    {
        if (!out || maxCount <= 0)
            return 0;

        uintptr_t gc = 0;
        uintptr_t player = 0;
        if (!resolveLocalPlayer(gc, player))
            return 0;

        const int32_t want = maxCount < off::kSkillRankCount ? maxCount : off::kSkillRankCount;
        int32_t count = 0;
        for (; count < want; ++count)
        {
            int32_t rank = 0;
            if (!mem::read(player + off::kSkillRanksOff + static_cast<uintptr_t>(count) * sizeof(int32_t), rank))
                break;

            out[count] = rank;
        }
        return count;
    }

    int32_t scanCorruptItems(ItemDefect* out, int32_t maxCount)
    {
        if (!out || maxCount <= 0)
            return 0;

        uintptr_t gc = 0;
        uintptr_t player = 0;
        if (!resolveLocalPlayer(gc, player))
            return 0;

        int32_t count = 0;

        // Held / cursor item first: count>0 with an invalid body is the crash-on-menu-open case.
        int32_t heldCount = 0;
        if (mem::read(player + off::kHeldItemCountOff, heldCount) && heldCount > 0)
        {
            const uintptr_t heldItem = player + off::kHeldItemOff;
            const char* reason = itemDefectReason(heldItem);

            if (reason && count < maxCount)
                out[count++] = makeDefect(heldItem, ItemDefectLocation::Held, -1, reason);
        }

        // Equipment: an occupied slot (type != 0, i.e. not the empty sentinel) that cannot render.
        for (int32_t slot = 0; slot < off::kEquipmentSlotCount && count < maxCount; ++slot)
        {
            const uintptr_t itemBase = player + off::kEquipmentBaseOff + static_cast<uintptr_t>(slot) * off::kEquipmentStride;
            uint8_t type = 0;
            if (!mem::read(itemBase + off::kItemTypeOff, type) || type == 0)
                continue;

            const char* reason = itemDefectReason(itemBase);
            if (reason)
                out[count++] = makeDefect(itemBase, ItemDefectLocation::Equipment, slot, reason);
        }

        // Inventory: an occupied cell (stack>0) with an invalid body. Same walk as listInventory.
        uint32_t outerBegin = 0;
        int32_t outerCount = 0;
        if (field::vectorSpan(player + off::kInventoryVecOff, off::kInventoryOuterStride, off::kInventoryMaxOuter, outerBegin, outerCount))
        {
            int32_t cellIndex = 0;
            for (int32_t o = 0; o < outerCount && count < maxCount; ++o)
            {
                const uintptr_t innerVec = outerBegin + static_cast<uintptr_t>(o) * off::kInventoryOuterStride;
                uint32_t innerBegin = 0;
                int32_t innerCount = 0;
                if (!field::vectorSpan(innerVec, off::kInventoryCellStride, off::kInventoryMaxInner, innerBegin, innerCount))
                    continue;

                for (int32_t j = 0; j < innerCount && count < maxCount; ++j, ++cellIndex)
                {
                    const uintptr_t cell = innerBegin + static_cast<uintptr_t>(j) * off::kInventoryCellStride;
                    int32_t stack = 0;

                    if (!mem::read(cell, stack) || stack <= 0)
                        continue;

                    const uintptr_t itemBase = cell + off::kInventoryCellItemOff;
                    const char* reason = itemDefectReason(itemBase);
                    if (reason)
                        out[count++] = makeDefect(itemBase, ItemDefectLocation::Inventory, cellIndex, reason);
                }
            }
        }
        return count;
    }

    // Writes one item field. Rejects/clamps edits that make the item unrenderable (unknown type
    // or modelless subtype -> "?" item that welds to cursor + crashes on draw); ranges from catalogs.
    bool setItemField(uint32_t itemAddress, int32_t fieldId, int32_t value)
    {
        const uintptr_t addr = static_cast<uintptr_t>(itemAddress);
        if (!addr || !mem::readable(reinterpret_cast<const void*>(addr), off::kItemStructSize))
            return false;

        // Current type/subtype give context: subtype validity depends on type, upgrade cap on 1H/2H.
        uint8_t curType = 0;
        uint8_t curSubtype = 0;

        mem::read(addr + off::kItemTypeOff, curType);
        mem::read(addr + off::kItemSubtypeOff, curSubtype);

        switch (fieldId)
        {
            case CUBE_ITEM_FIELD_TYPE:
            {
                // Unknown type (0 included) is THE corrupting edit: no model/name, welds to cursor. Reject.
                if (!isKnownCatalogId(CUBE_CATALOG_ITEM_TYPE, value))
                    return false;
                if (!mem::write<uint8_t>(addr + off::kItemTypeOff, static_cast<uint8_t>(value)))
                    return false;
                // Keep type/subtype coherent: a mismatched subtype has no model. Keep a valid
                // weapon subtype for weapons; otherwise reset to the universal subtype 0.
                {
                    const int32_t coherent = (value == off::kItemTypeWeapon && weaponSubtypeKnown(static_cast<int32_t>(curSubtype)))
                                                 ? static_cast<int32_t>(curSubtype) : off::kItemDefaultSubtype;
                    if (coherent != static_cast<int32_t>(curSubtype))
                        mem::write<uint8_t>(addr + off::kItemSubtypeOff, static_cast<uint8_t>(coherent));
                }
                return true;
            }
            case CUBE_ITEM_FIELD_SUBTYPE:
                if (!subtypeAcceptable(static_cast<int32_t>(curType), value))
                    return false;
                return mem::write<uint8_t>(addr + off::kItemSubtypeOff, static_cast<uint8_t>(value));
            case CUBE_ITEM_FIELD_MATERIAL:
                // Not a crash vector, but hold material to the known set (unnamed = unrealistic).
                if (!isKnownCatalogId(CUBE_CATALOG_MATERIAL, value))
                    return false;
                return mem::write<uint8_t>(addr + off::kItemMaterialOff, static_cast<uint8_t>(value));
            case CUBE_ITEM_FIELD_MODIFIER:
                return mem::write<uint8_t>(addr + off::kItemModifierOff,
                    static_cast<uint8_t>(clampInt(value, off::kItemModifierMin, off::kItemModifierMax)));
            case CUBE_ITEM_FIELD_LEVEL:
                return mem::write<int16_t>(addr + off::kItemLevelOff,
                    static_cast<int16_t>(clampInt(value, off::kItemLevelMin, off::kItemLevelMax)));
            case CUBE_ITEM_FIELD_UPGRADE_COUNT:
                return mem::write<int32_t>(addr + off::kItemUpgradeCountOff,
                    clampInt(value, off::kItemUpgradeMin, maxUpgradesFor(curType, curSubtype)));
            // Seed only feeds stat-roll variance (consumed modulo), so any value is safe.
            case CUBE_ITEM_FIELD_SEED: return mem::write<uint32_t>(addr + off::kItemSeedOff, static_cast<uint32_t>(value));
            // Inventory stack count sits in the cell, immediately before the item body.
            case CUBE_ITEM_FIELD_STACK:
                return mem::write<int32_t>(addr - off::kInventoryCellItemOff,
                    clampInt(value, off::kItemStackMin, off::kItemStackMax));
            default: return false;
        }
    }

    bool setSkillRank(int32_t index, int32_t value)
    {
        if (index < 0 || index >= off::kSkillRankCount)
            return false;
        uintptr_t gc = 0;
        uintptr_t player = 0;
        if (!resolveLocalPlayer(gc, player))
            return false;
        return mem::write<int32_t>(player + off::kSkillRanksOff + static_cast<uintptr_t>(index) * sizeof(int32_t), value);
    }

    namespace
    {
        // DFS over the MSVC std::map<int,int> ability-cooldown tree (bounded node count).
        // Children are read before fn runs, so an in-fn value write does not disturb traversal.
        template <typename Fn>
        void forEachAbilityNode(uintptr_t player, Fn fn)
        {
            uint32_t head = 0;
            if (!mem::read(player + off::kAbilityCdMapHeadOff, head) || !head)
                return;
            uint32_t root = 0;
            if (!mem::read(head + off::kRbParent, root) || !root || root == head)
                return; // empty map (root == the nil head sentinel)

            uint32_t stack[off::kMaxAbilityWalk];
            uint32_t sp = 0;
            stack[sp++] = root;
            uint32_t steps = 0;

            while (sp > 0 && steps < off::kMaxAbilityWalk)
            {
                ++steps;
                const uint32_t node = stack[--sp];
                if (!node || node == head)
                    continue;

                uint32_t left = 0;
                uint32_t right = 0;

                mem::read(node + off::kRbLeft, left);
                mem::read(node + off::kRbRight, right);

                int32_t key = 0;
                int32_t value = 0;

                mem::read(node + off::kAbilityCdKeyOff, key);
                mem::read(node + off::kAbilityCdValueOff, value);

                fn(node, key, value);
                if (left && left != head && sp < off::kMaxAbilityWalk)
                    stack[sp++] = left;
                if (right && right != head && sp < off::kMaxAbilityWalk)
                    stack[sp++] = right;
            }
        }

    }

    int32_t listAbilityCooldowns(CubeAbilityCooldown* out, int32_t maxCount)
    {
        if (!out || maxCount <= 0)
            return 0;

        uintptr_t gc = 0;
        uintptr_t player = 0;

        if (!resolveLocalPlayer(gc, player))
            return 0;

        int32_t count = 0;

        forEachAbilityNode(player, [&](uint32_t node, int32_t key, int32_t value)
        {
            if (count >= maxCount)
                return;

            CubeAbilityCooldown& cd = out[count];

            cd.structSize = sizeof(CubeAbilityCooldown);
            cd.address = node;
            cd.abilityId = key;
            cd.remainingMs = value;
            ++count;
        });
        return count;
    }

    bool setAbilityCooldown(int32_t abilityId, int32_t remainingMs)
    {
        uintptr_t gc = 0;
        uintptr_t player = 0;
        if (!resolveLocalPlayer(gc, player))
            return false;

        uint32_t targetNode = 0;
        forEachAbilityNode(player, [&](uint32_t node, int32_t key, int32_t)
        {
            if (key == abilityId)
                targetNode = node;
        });

        if (!targetNode)
            return false;

        return mem::write<int32_t>(targetNode + off::kAbilityCdValueOff, remainingMs);
    }

    int32_t clearAbilityCooldowns()
    {
        uintptr_t gc = 0;
        uintptr_t player = 0;
        if (!resolveLocalPlayer(gc, player))
            return 0;

        int32_t cleared = 0;
        forEachAbilityNode(player, [&](uint32_t node, int32_t, int32_t value)
        {
            if (value != 0 && mem::write<int32_t>(node + off::kAbilityCdValueOff, 0))
                ++cleared;
        });
        return cleared;
    }
}