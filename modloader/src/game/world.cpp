#include "game/world.h"
#include "game/gamecontroller.h"
#include "game/offsets.h"
#include "util/field.h"
#include "game/catalog.h"
#include "core/mem.h"

#include <cstdint>

namespace game
{
    namespace
    {
        constexpr int32_t kMsPerHour = 3600000;
        constexpr int32_t kMsPerMinute = 60000;
        constexpr int32_t kMinutesPerHour = 60;
        constexpr int32_t kHoursPerDay = 24;
        constexpr int32_t kMsPerDay = kMsPerHour * kHoursPerDay;
        constexpr uint8_t kSpawnBound = 1;

        // Player's current tile from the Creature position; false if unreadable.
        bool playerTile(uintptr_t creature, int32_t& tileX, int32_t& tileY)
        {
            int64_t rawX = 0;
            int64_t rawY = 0;

            if (!mem::read(creature + off::kPlayerPosXOff, rawX) || !mem::read(creature + off::kPlayerPosYOff, rawY))
                return false;

            tileX = static_cast<int32_t>(rawX >> off::kPosToTileShift);
            tileY = static_cast<int32_t>(rawY >> off::kPosToTileShift);
            return true;
        }

        // Walks World -> Region -> Zone for the player's tile. 0 if not resident.
        uintptr_t resolveZone(uintptr_t gc, int32_t tileX, int32_t tileY)
        {
            const uintptr_t world = gc + off::kWorldEmbedOff;
            const int32_t zoneX = tileX >> off::kTileToZoneShift;
            const int32_t zoneY = tileY >> off::kTileToZoneShift;
            const int32_t regionX = zoneX >> off::kZoneToRegionShift;
            const int32_t regionY = zoneY >> off::kZoneToRegionShift;

            if (regionX < 0 || regionX >= off::kRegionGridDim || regionY < 0 || regionY >= off::kRegionGridDim)
                return 0;

            const int32_t regionIdx = regionX * off::kRegionGridDim + regionY;
            uint32_t region = 0;
            if (!mem::read(world + off::kRegionGridOff + static_cast<uintptr_t>(regionIdx) * sizeof(uint32_t), region) || !region)
                return 0;

            const int32_t zoneMask = off::kZoneGridDim - 1;
            const int32_t zoneIdx = (zoneX & zoneMask) * off::kZoneGridDim + (zoneY & zoneMask);
            uint32_t zone = 0;
            if (!mem::read(region + off::kZoneGridOff + static_cast<uintptr_t>(zoneIdx) * sizeof(uint32_t), zone) || !zone)
                return 0;

            return zone;
        }

        // Resolves the ZoneTile for the tile the player stands on, or 0 if not resident.
        uintptr_t resolveTile(uintptr_t gc, int32_t tileX, int32_t tileY)
        {
            const uintptr_t zone = resolveZone(gc, tileX, tileY);
            if (!zone)
                return 0;

            uint32_t tileBase = 0;
            if (!mem::read(zone + off::kZoneTileBaseOff, tileBase) || !tileBase)
                return 0;

            const int32_t tileMask = off::kTileDim - 1;
            const int32_t tileIdx = (tileX & tileMask) + (tileY & tileMask) * off::kTileDim;
            const uintptr_t tile = tileBase + static_cast<uintptr_t>(tileIdx) * off::kTileStride;

            if (!mem::readable(reinterpret_cast<const void*>(tile), off::kTileStride))
                return 0;

            return tile;
        }

        void readClimate(uintptr_t tile, CubeWorld& out)
        {
            uint8_t terrain = 0;
            if (mem::read(tile + off::kTileTerrainOff, terrain))
                out.terrainType = static_cast<int32_t>(terrain);

            mem::read(tile + off::kTileTempOff, out.temperature);
            mem::read(tile + off::kTileHumidityOff, out.humidity);

            int32_t elevation = 0;
            if (mem::read(tile + off::kTileElevationOff, elevation))
                out.elevation = static_cast<float>(elevation);
            out.hasClimate = 1;
        }

    }

    bool readWorld(CubeWorld& out)
    {
        uintptr_t gc = 0;
        uintptr_t creature = 0;
        if (!resolveLocalPlayer(gc, creature))
            return false;

        out.address = static_cast<uint32_t>(gc);

        int32_t tileX = 0;
        int32_t tileY = 0;
        if (playerTile(creature, tileX, tileY))
        {
            out.zoneX = tileX >> off::kTileToZoneShift;
            out.zoneY = tileY >> off::kTileToZoneShift;
            out.regionX = out.zoneX >> off::kZoneToRegionShift;
            out.regionY = out.zoneY >> off::kZoneToRegionShift;
            out.hasArea = 1;

            const uintptr_t tile = resolveTile(gc, tileX, tileY);
            if (tile)
                readClimate(tile, out);
        }

        int32_t timeMs = 0;
        if (mem::read(gc + off::kTimeOfDayOff, timeMs))
        {
            int32_t dayMs = timeMs % kMsPerDay;
            if (dayMs < 0)
                dayMs += kMsPerDay;

            out.timeMs = timeMs;
            out.hour = dayMs / kMsPerHour;
            out.minute = (dayMs / kMsPerMinute) % kMinutesPerHour;
            out.isDay = (out.hour >= off::kDayStartHour && out.hour < off::kNightStartHour) ? 1 : 0;
            out.hasTime = 1;
        }

        // Bound spawn: int32 zone coords on the Creature (Z not stored). Gated on the bind flag.
        uint8_t spawnFlag = 0;
        int32_t spawnZoneX = 0;
        int32_t spawnZoneY = 0;

        if (mem::read(creature + off::kSpawnFlagOff, spawnFlag) && spawnFlag &&
            mem::read(creature + off::kSpawnZoneXOff, spawnZoneX) &&
            mem::read(creature + off::kSpawnZoneYOff, spawnZoneY) && (spawnZoneX || spawnZoneY))
        {
            out.spawnX = static_cast<float>(spawnZoneX) * static_cast<float>(off::kTileDim);
            out.spawnY = static_cast<float>(spawnZoneY) * static_cast<float>(off::kTileDim);
            out.spawnZ = 0.0f;
            out.hasSpawn = 1;
        }

        uint32_t seed = 0;
        if (mem::read(gc + off::kWorldSeedOff, seed))
        {
            out.seed = seed;
            out.hasSeed = 1;
        }
        return true;
    }

    int32_t listStructures(CubeStructure* out, int32_t maxCount)
    {
        if (!out || maxCount <= 0)
            return 0;

        uintptr_t gc = 0;
        uintptr_t creature = 0;
        if (!resolveLocalPlayer(gc, creature))
            return 0;

        int32_t tileX = 0;
        int32_t tileY = 0;
        if (!playerTile(creature, tileX, tileY))
            return 0;

        const uintptr_t zone = resolveZone(gc, tileX, tileY);
        if (!zone)
            return 0;

        uint32_t begin = 0;
        int32_t total = 0;
        if (!field::vectorSpan(zone + off::kZoneStructVecOff, off::kZoneStructStride, off::kZoneStructMaxWalk, begin, total))
            return 0;

        int32_t count = 0;
        for (int32_t i = 0; i < total && count < maxCount; ++i)
        {
            const uintptr_t record = begin + static_cast<uintptr_t>(i) * off::kZoneStructStride;
            int32_t type = 0;
            if (!mem::read(record + off::kStructTypeOff, type))
                continue;

            CubeStructure& s = out[count];
            s = CubeStructure{}; // caller buffer may be uninitialized
            s.structSize = sizeof(CubeStructure);
            s.address = static_cast<uint32_t>(record);
            s.type = type;
            catalogNameOr(CUBE_CATALOG_STRUCTURE_TYPE, type, s.name, sizeof(s.name), "structure");
            field::vec3Fixed64(record, off::kStructPosXOff, off::kPlayerPosScale, s.x, s.y, s.z);
            ++count;
        }
        return count;
    }

    bool setWorldTime(int32_t ms)
    {
        uintptr_t gc = 0;
        uintptr_t creature = 0;

        if (!resolveLocalPlayer(gc, creature))
            return false;

        int32_t wrapped = ms % kMsPerDay;
        if (wrapped < 0)
            wrapped += kMsPerDay;

        return mem::write<int32_t>(gc + off::kTimeOfDayOff, wrapped);
    }

    bool setWorldTile(int32_t fieldId, double value)
    {
        uintptr_t gc = 0;
        uintptr_t creature = 0;

        if (!resolveLocalPlayer(gc, creature))
            return false;

        int32_t tileX = 0;
        int32_t tileY = 0;

        if (!playerTile(creature, tileX, tileY))
            return false;

        const uintptr_t tile = resolveTile(gc, tileX, tileY);
        if (!tile)
            return false;

        switch (fieldId)
        {
            case CUBE_TILE_TERRAIN:
                return mem::write<uint8_t>(tile + off::kTileTerrainOff, static_cast<uint8_t>(value));
            case CUBE_TILE_TEMPERATURE:
                return mem::write<float>(tile + off::kTileTempOff, static_cast<float>(value));
            case CUBE_TILE_HUMIDITY:
                return mem::write<float>(tile + off::kTileHumidityOff, static_cast<float>(value));
            case CUBE_TILE_ELEVATION:
                return mem::write<int32_t>(tile + off::kTileElevationOff, static_cast<int32_t>(value));
            default: return false;
        }
    }

    bool setWorldSeed(uint32_t seed)
    {
        uintptr_t gc = 0;
        uintptr_t creature = 0;
        if (!resolveLocalPlayer(gc, creature))
            return false;

        return mem::write<uint32_t>(gc + off::kWorldSeedOff, seed);
    }

    bool setWorldSpawn(float x, float y, float z)
    {
        static_cast<void>(z); // spawn Z is derived from terrain, not stored
        uintptr_t gc = 0;
        uintptr_t creature = 0;

        if (!resolveLocalPlayer(gc, creature))
            return false;

        const int32_t zoneX = static_cast<int32_t>(x / static_cast<float>(off::kTileDim));
        const int32_t zoneY = static_cast<int32_t>(y / static_cast<float>(off::kTileDim));

        bool ok = mem::write<int32_t>(creature + off::kSpawnZoneXOff, zoneX);
        ok = mem::write<int32_t>(creature + off::kSpawnZoneYOff, zoneY) && ok;
        // Mark the spawn bound so readWorld (which gates on the flag) reports it.
        ok = mem::write<uint8_t>(creature + off::kSpawnFlagOff, kSpawnBound) && ok;
        return ok;
    }

    bool setStructureField(uint32_t address, int32_t fieldId, double value)
    {
        const uintptr_t record = static_cast<uintptr_t>(address);
        if (!record || !mem::readable(reinterpret_cast<const void*>(record), off::kZoneStructStride))
            return false;

        const float f = static_cast<float>(value);
        switch (fieldId)
        {
            case CUBE_STRUCT_TYPE:
                return mem::write<int32_t>(record + off::kStructTypeOff, static_cast<int32_t>(value));
            case CUBE_STRUCT_POS_X:
                return field::setFixed64(record, off::kStructPosXOff, off::kPlayerPosScale, f);
            case CUBE_STRUCT_POS_Y:
                return field::setFixed64(record, off::kStructPosXOff + sizeof(int64_t), off::kPlayerPosScale, f);
            case CUBE_STRUCT_POS_Z:
                return field::setFixed64(record, off::kStructPosXOff + sizeof(int64_t) * 2, off::kPlayerPosScale, f);
            default: return false;
        }
    }
}