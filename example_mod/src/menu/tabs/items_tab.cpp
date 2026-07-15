#include "menu/tabs/items_tab.h"
#include "mod_context.h"

#include "imgui.h"

#include <cstdio>
#include <vector>

namespace exmod::menu
{
    namespace
    {

        constexpr int kIdBufferSize = 24;
        constexpr int kMaxInventoryRows = 24;

        constexpr int kNoSubtypeCatalog = -1;

        // The catalog that names an item type's subtype (so the editor shows a dropdown).
        int subtypeCatalogForType(int type)
        {
            switch (static_cast<cube::ItemType>(type))
            {
                case cube::ItemType::Consumable: return CUBE_CATALOG_CONSUMABLE_SUBTYPE;
                case cube::ItemType::Weapon: return CUBE_CATALOG_WEAPON_SUBTYPE;
                case cube::ItemType::Special: return CUBE_CATALOG_SPECIAL_SUBTYPE;
                case cube::ItemType::Food: return CUBE_CATALOG_FOOD_SUBTYPE;
                case cube::ItemType::Accessory: return CUBE_CATALOG_ACCESSORY_SUBTYPE;
                case cube::ItemType::Vehicle: return CUBE_CATALOG_VEHICLE_SUBTYPE;
                default: return kNoSubtypeCatalog;
            }
        }

    }

    // strId is a STABLE per item id (slot / bag index) so changing a dropdown's label never changes
    // the node's ImGui id or collapses it. The composed name is only display text.
    void ItemsTab::drawItemNode(const cube::Item& item, const char* strId)
    {
        // The resolved item name (the full directory); prefix with material where it has one.
        const char* name = item.getName();
        const char* material = cube::catalog::name(g_api, CUBE_CATALOG_MATERIAL, item.getMaterial());
        char display[kValueBufferSize];
        if (material)
            std::snprintf(display, sizeof(display), "%s %s", material, name);
        else
            std::snprintf(display, sizeof(display), "%s", name);
        if (!ImGui::TreeNode(strId, "%s", display))
            return;
        if (beginTable("item_detail"))
        {
            row("Name", "%s", item.getName());
            row("Type", "%s", cube::catalog::nameOr(g_api, CUBE_CATALOG_ITEM_TYPE, item.getType(), item.getTypeName()));
            row("Value", "%d coins (sell %d)", item.getValue(), item.getSellValue());
            if (item.isConsumable() || item.isFood())
                row("Amount", "%d", item.getAmount());
            row("Slot", "%d", item.getSlot());
            row("Address", "0x%08X", item.raw().address);
            ImGui::EndTable();
        }
        int value = 0;
        if (idEditor("type", CUBE_CATALOG_ITEM_TYPE, item.getType(), value))
            item.setType(value);
        // Show a subtype dropdown from the type's subtype catalog when one exists.
        const int subtypeCatalog = subtypeCatalogForType(item.getType());
        if (subtypeCatalog != kNoSubtypeCatalog)
        {
            if (idEditor("subtype", static_cast<CubeCatalog>(subtypeCatalog), item.getSubtype(), value))
                item.setSubtype(value);
        }
        if (idEditor("material", CUBE_CATALOG_MATERIAL, item.getMaterial(), value))
            item.setMaterial(value);
        if (idEditor("modifier", CUBE_CATALOG_ITEM_MODIFIER, item.getModifier(), value))
            item.setModifier(value);
        if (item.getSlot() < 0) // inventory items carry a stack count; equipment is always 1
        {
            int stack = item.getStack();
            ImGui::SetNextItemWidth(sc(kInputWidth));
            if (ImGui::DragInt("stack", &stack, kIntDragSpeed, kSmallCountMin, kSmallCountMax, "%d", kClampFlags))
                item.setStack(stack);
        }
        int level = item.getLevel();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::DragInt("level", &level, kIntDragSpeed, kSmallCountMin, kSmallCountMax, "%d", kClampFlags))
            item.setLevel(level);
        int upgrades = item.getUpgradeCount();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::DragInt("upgrades", &upgrades, kIntDragSpeed, kSmallCountMin, kSmallCountMax, "%d", kClampFlags))
            item.setUpgradeCount(upgrades);
        int seed = static_cast<int>(item.getSeed());
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::DragInt("seed", &seed))
            item.setSeed(static_cast<unsigned>(seed));
        ImGui::TreePop();
    }

    void ItemsTab::drawEquipment()
    {
        const std::vector<cube::Item> equipped = cube::equipmentOf(g_api);
        if (equipped.empty())
        {
            ImGui::TextDisabled("(none equipped)");
            return;
        }
        for (const cube::Item& item : equipped)
        {
            char id[kIdBufferSize];
            std::snprintf(id, sizeof(id), "eq%d", item.getSlot());
            drawItemNode(item, id);
        }
    }

    void ItemsTab::drawBag()
    {
        const std::vector<cube::Item> bag = cube::inventoryOf(g_api);
        const int total = static_cast<int>(bag.size());
        const int shownCount = total < kMaxInventoryRows ? total : kMaxInventoryRows;
        if (shownCount < total)
            ImGui::Text("showing %d of %d items (capped)", shownCount, total);
        else
            ImGui::Text("%d items", total);
        for (int i = 0; i < shownCount; ++i)
        {
            char id[kIdBufferSize];
            std::snprintf(id, sizeof(id), "bag%d", i);
            drawItemNode(bag[static_cast<size_t>(i)], id);
        }
    }

    void ItemsTab::drawSkills()
    {
        const std::vector<int> skills = cube::skillsOf(g_api);
        cube::Hero hero(g_api);
        if (skills.empty())
        {
            ImGui::TextDisabled("(unavailable)");
            return;
        }
        ImGui::TextDisabled("edit points spent per skill:");
        for (size_t i = 0; i < skills.size(); ++i)
        {
            const char* skillName = cube::catalog::name(g_api, CUBE_CATALOG_SKILL, static_cast<int>(i));
            char label[kValueBufferSize];
            if (skillName)
                std::snprintf(label, sizeof(label), "%s", skillName);
            else
                std::snprintf(label, sizeof(label), "skill %d", static_cast<int>(i));
            int rank = skills[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::SetNextItemWidth(sc(kInputWidth));
            if (ImGui::DragInt(label, &rank, kIntDragSpeed, kSmallCountMin, kSmallCountMax, "%d", kClampFlags) && hero.valid())
                hero.setSkillRank(static_cast<int>(i), rank);
            ImGui::PopID();
        }
    }

    void ItemsTab::draw(const CubeEventArgs&)
    {
        if (!ImGui::BeginTabBar("##itemtabs"))
            return;
        if (ImGui::BeginTabItem("Equipment"))
        {
            drawEquipment();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Bag"))
        {
            drawBag();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Skills"))
        {
            drawSkills();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

}
