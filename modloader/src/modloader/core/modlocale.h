#pragma once
// Per-mod localization: one flat ini per (mod, locale) at <dllDir>/lang/<stem>/<locale>.ini, keyed by
// the mod's DLL stem (like modconfig). A translate resolves the mod's active locale (default from the
// loader - env CUBE_MOD_LOCALE, else "en" - and per-mod overridable via setLocale), reads the key
// case-insensitively, and falls back to the caller's fallback. Lazily loaded + cached per (stem,
// locale) in memory, so a per-frame translate never hits disk.
#include <string>

namespace modloader::modlocale
{
    // Set the locale root (<dllDir>/lang), create it, and pick the default active locale. Call once at
    // install, before any mod init (a mod may translate a startup string in its init).
    void init(const std::string& dllDir);

    // Translate key in modStem's active locale; returns fallback when the key is missing/unresolved.
    std::string translate(const std::string& modStem, const std::string& key, const std::string& fallback);
    // Set modStem's active locale (which <locale>.ini subsequent translate calls read); empty = default.
    void setLocale(const std::string& modStem, const std::string& locale);
    // The mod's active locale (the loader default until the mod overrides it).
    std::string getLocale(const std::string& modStem);
}
