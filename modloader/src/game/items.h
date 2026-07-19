#pragma once
// Player equipment, inventory (vector-of-vectors), skills and item editing.

#include "cube_sdk.h"
#include <cstdint>

namespace game
{

    int32_t listEquipment(CubeItem* out, int32_t maxCount);
    int32_t listInventory(CubeItem* out, int32_t maxCount);
    // Inventory of any creature at creatureAddress (validated first); e.g. a merchant's stock.
    int32_t listInventoryOf(uint32_t creatureAddress, CubeItem* out, int32_t maxCount);
    // The item's coin value via the game price function (buy price; sell = value/2). 0 if unreadable.
    int32_t itemValue(uint32_t itemAddress);
    int32_t listSkillRanks(int32_t* out, int32_t maxCount);
    // Live per-ability cooldown timers (std::map<int,int> @Creature+kAbilityCdMapHeadOff).
    int32_t listAbilityCooldowns(CubeAbilityCooldown* out, int32_t maxCount);
    // Sets abilityId's remaining cooldown (ms); false if absent from the map / unavailable.
    bool setAbilityCooldown(int32_t abilityId, int32_t remainingMs);
    // Zeroes every live ability cooldown; returns the number cleared.
    int32_t clearAbilityCooldowns();
    // Writes one CubeItemField of the item at itemAddress (equipment or inventory).
    bool setItemField(uint32_t itemAddress, int32_t fieldId, double value);
    // Sets skill rank [index] (0..kSkillRankCount-1) on the local player.
    bool setSkillRank(int32_t index, int32_t value);
    // Resolves an item (type, subtype) to its display name via the extracted item-name registry.
    // Never null: Food falls back to "Bait", singleton types to the type name, unknown to "item".
    const char* itemDisplayName(int32_t type, int32_t subtype);
    // Reads the 0x118-byte Item POD at itemBase; true if a non-empty item (type != 0) present.
    bool readItem(uintptr_t itemBase, CubeItem& out);
    // Reads a single equipment slot (0..11) off any Creature; true if a weapon is present.
    bool readEquipmentSlot(uintptr_t creature, int32_t slot, CubeItem& out);

    // Where a detected corrupt item lives.
    enum class ItemDefectLocation
    {
        Held, // on the cursor - the crash-on-menu-open case
        Equipment,
        Inventory
    };

    // A present-but-unrenderable item (shows as "?", welds to cursor, can crash on draw).
    struct ItemDefect
    {
        uint32_t address; // item body base
        ItemDefectLocation location;
        int32_t slot; // equipment slot, inventory cell index, or -1 (held)
        int32_t type;
        int32_t subtype;
        const char* reason;
    };

    // Scans held/equipment/inventory for corrupt items into out[] (up to maxCount). Silent.
    int32_t scanCorruptItems(ItemDefect* out, int32_t maxCount);

}
