#pragma once
// Assets: the mod-facing asset-override facade. Registers this mod's bytes for a game asset addressed by
// its original filename key (e.g. "aim.png", "alga.cub"); the loader owns the storage-format encoding
// and the game-thread detour that serves the override. Requires the Assets capability and a compatible
// game build. Get with mod.assets().

#include "cube/common.hpp"

#include <cstdint>
#include <vector>

namespace cube
{
    class Assets
    {
    public:
        explicit Assets(const CubeApi* api = nullptr) : m_api(api) {}

        // Register (replace) this mod's override for key with size bytes copied from data. Returns false
        // on bad args, a missing capability, or an incompatible game build.
        bool set(const char* key, const void* data, int size) const
        {
            return m_api && m_api->assets.registerAsset(m_api, key, data, static_cast<int32_t>(size)) != 0;
        }

        bool set(const char* key, const std::vector<uint8_t>& bytes) const
        {
            return set(key, bytes.data(), static_cast<int>(bytes.size()));
        }

        // Remove this mod's override for key. Returns true if one existed.
        bool remove(const char* key) const
        {
            return m_api && m_api->assets.unregisterAsset(m_api, key) != 0;
        }

        // Whether this mod currently overrides key.
        bool has(const char* key) const
        {
            return m_api && m_api->assets.hasAsset(m_api, key) != 0;
        }

    private:
        const CubeApi* m_api;
    };
}
