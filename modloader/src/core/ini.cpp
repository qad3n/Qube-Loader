#include "core/ini.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace ini
{

    std::string lower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    std::string trim(const std::string& s)
    {
        const size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos)
            return "";
        const size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    bool parseBool(const std::string& v, bool& out)
    {
        const std::string s = lower(trim(v));
        if (s == "1" || s == "true" || s == "yes" || s == "on")
        {
            out = true;
            return true;
        }
        if (s == "0" || s == "false" || s == "no" || s == "off")
        {
            out = false;
            return true;
        }
        return false;
    }

    KeyValues read(const std::string& path)
    {
        KeyValues kv;
        std::ifstream f(path);
        std::string line;
        while (std::getline(f, line))
        {
            const std::string t = trim(line);
            if (t.empty() || t[0] == '#' || t[0] == ';' || t[0] == '[')
                continue;
            const size_t eq = t.find('=');
            if (eq == std::string::npos)
                continue;
            kv[lower(trim(t.substr(0, eq)))] = trim(t.substr(eq + 1));
        }
        return kv;
    }

    bool write(const std::string& path, const KeyValues& kv)
    {
        std::ofstream f(path, std::ios::out | std::ios::trunc);
        if (!f.is_open())
            return false;
        for (const KeyValues::value_type& entry : kv)
            f << entry.first << '=' << entry.second << '\n';
        return f.good();
    }

}
