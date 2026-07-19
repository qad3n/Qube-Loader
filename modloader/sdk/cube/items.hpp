#pragma once
// Item accessors: ItemType enum, Item.

#include "cube/common.hpp"

namespace cube
{
    enum class ItemType
    {
        Empty = 0,
        Consumable = 1, // potions + edible consumables
        Formula = 2,
        Weapon = 3,
        ChestArmor = 4,
        Gloves = 5,
        Boots = 6,
        ShoulderArmor = 7,
        Amulet = 8,
        Ring = 9,
        Special = 11, // crafting material / monster part
        Coin = 12,
        PlatinumCoin = 13,
        Candle = 18,
        Pet = 19,
        Food = 20, // food; also the pet-taming / pet-food item
        Accessory = 21, // medical / trinket
        Vehicle = 23,
        Lamp = 24,
        ManaCube = 25
    };

    // A single item (an equipped item or an inventory item). Value type over a snapshot.
    class Item
    {
    public:
        Item() = default;
        explicit Item(const CubeItem& data, const CubeApi* api = nullptr) : m_data(data), m_api(api) {}

        bool present() const { return m_data.present != 0; }
        int getSlot() const { return m_data.slot; } // equipment slot, or -1 for inventory
        int getStack() const { return m_data.stack; }
        int getType() const { return m_data.type; }
        int getSubtype() const { return m_data.subtype; }
        int getMaterial() const { return m_data.material; }
        int getModifier() const { return m_data.modifier; }
        int getLevel() const { return m_data.level; }
        unsigned getSeed() const { return m_data.seed; }
        int getUpgradeCount() const { return m_data.upgradeCount; }
        const char* getTypeName() const { return m_data.typeName; } // coarse category
        const char* getName() const { return m_data.name; } // resolved item name (the full directory)
        ItemType getItemType() const { return static_cast<ItemType>(m_data.type); }
        // Consumables and food store their heal / nourishment amount in the level field.
        int getAmount() const { return m_data.level; }
        bool isWeapon() const { return getItemType() == ItemType::Weapon; }
        bool isConsumable() const { return getItemType() == ItemType::Consumable; }
        bool isFood() const { return getItemType() == ItemType::Food; }
        bool isPetFood() const { return isFood(); } // pet-taming feeds a Food item to a tameable critter
        // Coin value via the game price function (this is the buy price). Needs an api-bound item.
        int getValue() const { return (m_api && m_data.address) ? static_cast<int>(m_api->items.value(m_api, m_data.address)) : 0; }
        // The sell price the game gives for this item (half the buy value).
        int getSellValue() const { return getValue() / kSellValueDivisor; }
        const CubeItem& raw() const { return m_data; }
        // Live edits (needs an api-bound item, i.e. from equipmentOf/inventoryOf).
        bool set(ItemField field, double value) const { return m_api && m_data.address && m_api->items.setField(m_api, m_data.address, static_cast<int32_t>(field), value) != 0; }
        bool setType(int type) const { return set(ItemField::Type, type); }
        bool setSubtype(int subtype) const { return set(ItemField::Subtype, subtype); }
        bool setMaterial(int material) const { return set(ItemField::Material, material); }
        bool setModifier(int modifier) const { return set(ItemField::Modifier, modifier); }
        bool setLevel(int level) const { return set(ItemField::Level, level); }
        bool setUpgradeCount(int count) const { return set(ItemField::UpgradeCount, count); }
        bool setSeed(unsigned seed) const { return set(ItemField::Seed, seed); }
        // Stack lives in the cell before the item body (inventory items only); writing it on an
        // equipment item would corrupt the neighbouring slot, so guard on slot < 0.
        bool setStack(int stack) const { return m_data.slot < 0 && set(ItemField::Stack, stack); }

    private:
        static constexpr int kSellValueDivisor = 2; // the game sells items for half the buy value
        CubeItem m_data = {};
        const CubeApi* m_api = nullptr;
    };

}
