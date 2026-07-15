#pragma once
// Per-mod save data: opaque binary blobs, one file per key at <dllDir>/data/<stem>/[<scope>/]<key>.bin,
// keyed by the mod's DLL stem (the stable pre-init identity; see modconfig). Distinct from modconfig
// (that is user-editable text settings; this is mod-owned, binary-safe save state - counters,
// discovered progress). An optional per-mod scope (world seed / character) namespaces a blob under a
// subdirectory so one mod can keep separate saves per game.
#include <cstdint>
#include <string>

namespace modloader::modstorage
{
    // Set the data root (<dllDir>/data) and create it. Call once at install, before any mod init.
    void init(const std::string& dllDir);

    // Set the mod's active scope subdirectory (empty = the mod's unscoped root). Held per DLL stem.
    void setScope(const std::string& modStem, const std::string& scope);

    // Copy up to size bytes of the blob at key into out; returns bytes read (0 if absent/unreadable).
    // out == nullptr probes: returns the stored blob size without reading.
    int32_t get(const std::string& modStem, const std::string& key, void* out, int32_t size);
    // Write size bytes as the blob at key (overwrites). Returns true on success.
    bool put(const std::string& modStem, const std::string& key, const void* data, int32_t size);
    // Delete the blob at key. Returns true if it existed and was removed.
    bool remove(const std::string& modStem, const std::string& key);
    // Whether a blob exists at key.
    bool has(const std::string& modStem, const std::string& key);
}
