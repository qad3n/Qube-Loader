#pragma once
// Asset override mechanism: a loader-owned game-thread detour on db::loadBlobByKey (the shared read
// choke-point for the data*.db blob stores) that returns a mod-supplied blob for a filename key. Like
// game::selection / game::pickup it targets a pinned address and is gated on a compatible Cube.exe
// build. The override table is a flat key -> plaintext map; the mod supplies decoded bytes and the
// loader re-encodes non-.cub data (via assetcodec) so the game's own decode reconstructs it. Per-mod
// ownership of overrides lives one layer up in modloader::modassets.

#include <cstdint>
#include <string>

namespace game::assets
{
    // Install/remove the loadBlobByKey detour. install() loads the codec key, runs a round-trip
    // self-test, and patches the target; it is a no-op returning false on a mismatched build or a failed
    // self-test. remove() flips to pass-through and drains in-flight calls before unpatching.
    bool install();
    void remove();

    // Whether asset injection can work on this build (compatible Cube.exe). Knowable at mod-init time,
    // before install(), so the bridge can reject registerAsset early on an incompatible build.
    bool available();

    // Set (replace) the override for key with size bytes copied from data (plaintext). Returns false on
    // bad args. Thread-safe against the game-thread reader. Storing is allowed before install(); a stored
    // override goes live once the detour is installed.
    bool setOverride(const std::string& key, const void* data, int32_t size);
    // Remove the override for key. Returns true if one existed.
    bool removeOverride(const std::string& key);
}
