#pragma once
// Per-mod ownership over the flat asset-override table in game::assets: tracks which DLL stem registered
// each filename key so a mod's overrides drop on unload, and warns on a cross-mod key collision (last
// writer wins). Keyed by DLL stem like modconfig/modstorage. Purely in-memory; the mod supplies the
// bytes through the assets API (there is no on-disk asset store yet).
#include <cstdint>
#include <string>

namespace modloader::modassets
{
    // Register (replace) modStem's override for key with size bytes. Returns false on bad args or when
    // asset injection is unavailable on this build.
    bool registerAsset(const std::string& modStem, const std::string& key, const void* data, int32_t size);
    // Withdraw modStem's override for key. Returns true if this stem owned it and it was removed.
    bool unregisterAsset(const std::string& modStem, const std::string& key);
    // Whether modStem currently owns an override for key.
    bool hasAsset(const std::string& modStem, const std::string& key);
    // Drop every override owned by modStem (on unload).
    void dropOwner(const std::string& modStem);
}
