#include "menu/tabs/world_tab.h"
#include "mod_context.h"

#include "imgui.h"

#include <vector>

namespace exmod::menu
{
    namespace
    {

        constexpr int kMaxHour = 23;
        constexpr int kMaxMinute = 59;
        constexpr int kMaxStructureRows = 16;

    }

    void WorldTab::drawLocation(cube::World& world)
    {
        if (beginTable("wd_loc"))
        {
            row("Zone", "%d, %d", world.getZoneX(), world.getZoneY());
            row("Region", "%d, %d", world.getRegionX(), world.getRegionY());
            ImGui::EndTable();
        }
        int seed = static_cast<int>(world.getSeed());
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::DragInt("seed", &seed))
            world.setSeed(static_cast<unsigned>(seed));
        if (world.hasSpawn())
        {
            const cube::Vec3 spawn = world.getSpawn();
            float point[3] = {spawn.x, spawn.y, spawn.z};
            ImGui::SetNextItemWidth(sc(kTeleportInputWidth));
            if (ImGui::DragFloat3("spawn x/y/z", point, kStatDragSpeed))
                world.setSpawn(point[0], point[1], point[2]);
            if (ImGui::Button("Teleport to spawn"))
            {
                cube::Hero hero(g_api);
                if (hero.valid())
                    hero.teleport(world.getSpawn());
            }
        }
    }

    void WorldTab::drawClimate(cube::World& world)
    {
        if (!world.hasClimate())
        {
            ImGui::TextDisabled("tile not resident");
            return;
        }
        ImGui::TextDisabled("edits the player's current ZoneTile:");
        float temperature = world.getTemperature();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::SliderFloat("temperature", &temperature, kResourceMin, kFullResource, "%.2f", kClampFlags))
            world.setTemperature(temperature);
        float humidity = world.getHumidity();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::SliderFloat("humidity", &humidity, kResourceMin, kFullResource, "%.2f", kClampFlags))
            world.setHumidity(humidity);
        float elevation = world.getElevation();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::DragFloat("elevation", &elevation, kStatDragSpeed))
            world.setElevation(elevation);
        int terrain = 0;
        if (idEditor("terrain", CUBE_CATALOG_TERRAIN, world.getTerrainType(), terrain))
            world.setTerrain(terrain);
    }

    void WorldTab::drawTime(cube::World& world)
    {
        if (!world.hasTime())
        {
            ImGui::TextDisabled("(unresolved)");
            return;
        }
        if (beginTable("wd_time"))
        {
            row("Clock", "%02d:%02d", world.getHour(), world.getMinute());
            row("Phase", "%s", world.isDay() ? "day" : "night");
            row("Time ms", "%d", world.getTimeMs());
            ImGui::EndTable();
        }
        ImGui::SeparatorText("set time of day");
        int hour = world.getHour();
        int minute = world.getMinute();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        const bool hourChanged = ImGui::SliderInt("hour", &hour, 0, kMaxHour, "%d", kClampFlags);
        ImGui::SetNextItemWidth(sc(kInputWidth));
        const bool minuteChanged = ImGui::SliderInt("minute", &minute, 0, kMaxMinute, "%d", kClampFlags);
        if (hourChanged || minuteChanged)
            world.setTimeOfDay(hour, minute);
    }

    void WorldTab::drawStructures()
    {
        const std::vector<cube::Structure> structures = cube::structuresOf(g_api);
        const int total = static_cast<int>(structures.size());
        const int shownCount = total < kMaxStructureRows ? total : kMaxStructureRows;
        if (shownCount < total)
            ImGui::Text("showing %d of %d in zone (capped)", shownCount, total);
        else
            ImGui::Text("%d in zone", total);
        cube::Hero hero(g_api);
        for (int i = 0; i < shownCount; ++i)
        {
            const cube::Structure& structure = structures[static_cast<size_t>(i)];
            const cube::Vec3 pos = structure.getPosition();
            ImGui::PushID(static_cast<int>(structure.raw().address));
            if (ImGui::TreeNode("row", "%s", structure.getName()))
            {
                ImGui::TextDisabled("record @ 0x%08X", structure.raw().address);
                int type = 0;
                if (idEditor("type", CUBE_CATALOG_STRUCTURE_TYPE, structure.getType(), type))
                    structure.setType(type);
                float position[3] = {pos.x, pos.y, pos.z};
                ImGui::SetNextItemWidth(sc(kTeleportInputWidth));
                if (ImGui::DragFloat3("position", position, kStatDragSpeed))
                    structure.setPosition(position[0], position[1], position[2]);
                if (ImGui::SmallButton("Teleport me here") && hero.valid())
                    hero.teleport(pos);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    void WorldTab::draw(const CubeEventArgs&)
    {
        cube::World world(g_api);
        if (!world.valid())
        {
            ImGui::TextDisabled("(unavailable)");
            return;
        }
        addressHeader("GameController", world.raw().address);
        if (!ImGui::BeginTabBar("##worldtabs"))
            return;
        if (ImGui::BeginTabItem("Location"))
        {
            drawLocation(world);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Climate"))
        {
            drawClimate(world);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Time"))
        {
            drawTime(world);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Structures"))
        {
            drawStructures();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

}
