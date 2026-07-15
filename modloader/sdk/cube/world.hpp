#pragma once
// World + placed-structure accessors: World, Structure.

#include "cube/common.hpp"

namespace cube
{
    // Day-clock conversion for World::setTimeOfDay (the game stores time as ms/day).
    constexpr int kMinutesPerHour = 60;
    constexpr int kSecondsPerMinute = 60;
    constexpr int kMillisPerSecond = 1000;

    class World
    {
    public:
        explicit World(const CubeApi* api) : m_api(api), m_valid(api && api->world.get(api, &m_data) != 0) {}

        bool valid() const { return m_valid; }
        bool refresh() { m_valid = m_api && m_api->world.get(m_api, &m_data) != 0; return m_valid; }
        bool reload() { return refresh(); }
        int getZoneX() const { return m_data.zoneX; }
        int getZoneY() const { return m_data.zoneY; }
        int getRegionX() const { return m_data.regionX; }
        int getRegionY() const { return m_data.regionY; }
        bool hasClimate() const { return m_data.hasClimate != 0; }
        float getTemperature() const { return m_data.temperature; }
        float getHumidity() const { return m_data.humidity; }
        float getElevation() const { return m_data.elevation; }
        int getTerrainType() const { return m_data.terrainType; }
        bool hasTime() const { return m_data.hasTime != 0; }
        int getTimeMs() const { return m_data.timeMs; }
        int getHour() const { return m_data.hour; }
        int getMinute() const { return m_data.minute; }
        bool isDay() const { return m_data.isDay != 0; }
        bool hasSpawn() const { return m_data.hasSpawn != 0; }
        Vec3 getSpawn() const { return Vec3{m_data.spawnX, m_data.spawnY, m_data.spawnZ}; }
        bool hasSeed() const { return m_data.hasSeed != 0; }
        unsigned getSeed() const { return m_data.seed; }
        const CubeWorld& raw() const { return m_data; }
        // Live edits: time-of-day and the player's current ZoneTile (climate/terrain).
        bool setTime(int ms) const { return m_api && m_api->world.setTime(m_api, ms) != 0; }
        bool setTimeOfDay(int hour, int minute) const { return setTime((hour * kMinutesPerHour + minute) * kSecondsPerMinute * kMillisPerSecond); }
        bool setTile(TileField field, double value) const { return m_api && m_api->world.setTile(m_api, static_cast<int32_t>(field), value) != 0; }
        bool setTemperature(float value) const { return setTile(TileField::Temperature, value); }
        bool setHumidity(float value) const { return setTile(TileField::Humidity, value); }
        bool setElevation(float value) const { return setTile(TileField::Elevation, value); }
        bool setTerrain(int terrain) const { return setTile(TileField::Terrain, terrain); }
        bool setSeed(unsigned seed) const { return m_api && m_api->world.setSeed(m_api, seed) != 0; }
        bool setSpawn(float x, float y, float z) const { return m_api && m_api->world.setSpawn(m_api, x, y, z) != 0; }
        bool setSpawn(const Vec3& point) const { return setSpawn(point.x, point.y, point.z); }

    private:
        const CubeApi* m_api;
        CubeWorld m_data = {};
        bool m_valid;
    };

    // A placed structure / POI / spawn candidate in the current zone.
    class Structure
    {
    public:
        Structure() = default;
        explicit Structure(const CubeStructure& data, const CubeApi* api = nullptr) : m_data(data), m_api(api) {}

        int getType() const { return m_data.type; }
        const char* getName() const { return m_data.name; } // resolved structure name
        Vec3 getPosition() const { return Vec3{m_data.x, m_data.y, m_data.z}; }
        const CubeStructure& raw() const { return m_data; }
        // Live edits to this structure record.
        bool setField(StructField field, double value) const { return m_api && m_data.address && m_api->world.setStructure(m_api, m_data.address, static_cast<int32_t>(field), value) != 0; }
        bool setType(int type) const { return setField(StructField::Type, type); }
        bool setPosition(float x, float y, float z) const { return setField(StructField::PosX, x) && setField(StructField::PosY, y) && setField(StructField::PosZ, z); }
        bool setPosition(const Vec3& to) const { return setPosition(to.x, to.y, to.z); }

    private:
        CubeStructure m_data = {};
        const CubeApi* m_api = nullptr;
    };

    inline std::vector<Structure> structuresOf(const CubeApi* api)
    {
        return detail::fillList<CubeStructure, Structure, CUBE_STRUCTURES_MAX>(api, api ? api->world.structures : nullptr);
    }

}
