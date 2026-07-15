#pragma once
// Flat key=value ini read/write + small string helpers, shared by the loader config and per-mod files.
#include <map>
#include <string>

namespace ini
{

    using KeyValues = std::map<std::string, std::string>;

    std::string lower(std::string s);
    std::string trim(const std::string& s);
    bool parseBool(const std::string& v, bool& out);

    KeyValues read(const std::string& path);
    bool write(const std::string& path, const KeyValues& kv);

}
