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

}
