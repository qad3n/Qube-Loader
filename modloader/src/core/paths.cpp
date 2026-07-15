#include "core/paths.h"

#include <windows.h>

namespace paths
{
    namespace
    {
        constexpr char kSanitizeFallback[] = "_";

        bool isSafeChar(char c)
        {
            return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                   c == '.' || c == '_' || c == '-';
        }
    }

    std::string sanitizeComponent(const std::string& raw)
    {
        std::string out;
        out.reserve(raw.size());
        for (char c : raw)
            out += isSafeChar(c) ? c : '_';

        // Neutralize a component that is only dots ("." / ".." / ...): those would traverse upward.
        if (out.find_first_not_of('.') == std::string::npos)
            return kSanitizeFallback;

        return out;
    }

    bool ensureDir(const std::string& dir)
    {
        if (dir.empty() || dir == ".")
            return true;

        if (CreateDirectoryA(dir.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS)
            return true;

        // A missing parent is the only recoverable failure: build it, then retry this level once.
        if (GetLastError() != ERROR_PATH_NOT_FOUND)
            return false;

        const std::string parent = parentDir(dir);
        if (parent == dir || !ensureDir(parent))
            return false;

        return CreateDirectoryA(dir.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
    }

}
