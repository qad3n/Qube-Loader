#pragma once
// Per-mod user-editable settings, one flat ini per mod at <dllDir>/config/<stem>.ini, keyed by the
// mod's DLL stem (like modregistry: the stem is the loader's stable identity that is known before a
// mod's init runs, unlike the manifest id it declares during init). Values are typed on the way
// in/out (int/float/bool/string) with a caller-supplied default on a missing or malformed key. Lazily
// loaded and cached in memory (a per-frame get never hits disk); a set updates the cache and writes
// the file through immediately (durable).
#include <cstdint>
#include <string>

namespace modloader::modconfig
{
    // Set the config root (<dllDir>/config) and create it. Call once at install, before any mod init.
    void init(const std::string& dllDir);

    int32_t getInt(const std::string& modStem, const std::string& key, int32_t fallback);
    double getFloat(const std::string& modStem, const std::string& key, double fallback);
    bool getBool(const std::string& modStem, const std::string& key, bool fallback);
    std::string getString(const std::string& modStem, const std::string& key, const std::string& fallback);

    bool setInt(const std::string& modStem, const std::string& key, int32_t value);
    bool setFloat(const std::string& modStem, const std::string& key, double value);
    bool setBool(const std::string& modStem, const std::string& key, bool value);
    bool setString(const std::string& modStem, const std::string& key, const std::string& value);
}
