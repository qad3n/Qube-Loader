#pragma once
// Filesystem path helpers.
#include <string>

namespace paths
{

    constexpr char kSeparator = '\\';
    constexpr char kAltSeparator = '/';
    constexpr char kSeparators[] = {kSeparator, kAltSeparator, '\0'};

    inline std::string join(const std::string& dir, const char* file)
    {
        std::string out = dir.empty() ? "." : dir;
        if (out.back() != kSeparator && out.back() != kAltSeparator)
            out += kSeparator;

        out += file;
        return out;
    }

    inline std::string parentDir(const std::string& path)
    {
        const size_t slash = path.find_last_of(kSeparators);
        if (slash == std::string::npos)
            return ".";

        return path.substr(0, slash);
    }

    // Sanitizes an untrusted string (mod id, config/storage key or scope) into a single safe path
    // component: keeps letters, digits, dot, underscore and hyphen, maps everything else to '_', and
    // neutralizes any "." / ".." so it can never escape its parent directory. Empty or dot only input
    // returns a single underscore.
    std::string sanitizeComponent(const std::string& raw);

    // Recursively creates dir (and any missing parents). Returns true if it exists afterward.
    bool ensureDir(const std::string& dir);

}
