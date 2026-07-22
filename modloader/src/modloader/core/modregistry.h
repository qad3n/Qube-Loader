#pragma once
// Persistent per mod enable/disable + fault strike record, stored in <dllDir>/mods.ini keyed by DLL
// stem. Loaded once at install (before scan), consulted by scan() to skip disabled mods, and updated
// by the fault policy. Stems are matched case insensitively (Windows filenames are).
#include <cstdint>
#include <string>

namespace modloader::modregistry
{
    // Read <dllDir>/mods.ini into memory. Call once at install, before scan(). Missing file = empty.
    void load(const std::string& dllDir);

    // Whether this DLL stem may load. Unknown stems default to enabled.
    bool isEnabled(const std::string& stem);

    // Record that a stem exists this session; a newly seen stem is added as enabled. Persists if changed.
    void noteSeen(const std::string& stem);

    // Force a stem enabled/disabled. Persists if changed.
    void setEnabled(const std::string& stem, bool enabled);

    // Increment a stem's fault strike count, disabling it once the count reaches the limit. Returns the
    // new count. Persists. Used by the disable on repeated fault policy.
    int32_t recordFault(const std::string& stem);
}
