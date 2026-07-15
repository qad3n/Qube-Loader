#include "core/config.h"
#include "core/log.h"
#include "core/paths.h"

#include <windows.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <string>

namespace config
{
    namespace
    {
        constexpr char kCategory[] = "cfg";
        constexpr char kIniFileName[] = "cube_mod.ini";
        constexpr int kEnvBufSize = 1024;

        struct Key
        {
            const char* env;
            const char* ini;
        };

        constexpr Key kLogLevel = {"CUBE_MOD_LOG_LEVEL", "log_level"};
        constexpr Key kLogDir = {"CUBE_MOD_LOG_DIR", "log_dir"};
        constexpr Key kConsole = {"CUBE_MOD_CONSOLE", "console"};
        constexpr Key kColor = {"CUBE_MOD_COLOR", "color"};
        constexpr Key kColorMode = {"CUBE_MOD_COLOR_MODE", "color_mode"};
        constexpr Key kCapture = {"CUBE_MOD_CAPTURE", "capture_game_log"};
        constexpr Key kFaultIsolation = {"CUBE_MOD_FAULT_ISOLATION", "fault_isolation"};

        using KeyValues = std::map<std::string, std::string>;

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

        KeyValues readIni(const std::string& path)
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

        std::string env(const char* name)
        {
            char buf[kEnvBufSize];
            const DWORD n = GetEnvironmentVariableA(name, buf, sizeof(buf));
            if (n > 0 && n < static_cast<DWORD>(sizeof(buf)))
                return std::string(buf, n);
            return std::string();
        }

        std::string resolve(const KeyValues& ini, const Key& k)
        {
            const std::string fromEnv = env(k.env);
            if (!fromEnv.empty())
                return fromEnv;
            const KeyValues::const_iterator it = ini.find(k.ini);
            if (it == ini.end())
                return std::string();
            return it->second;
        }

        void recordReject(Settings& s, const Key& k, const std::string& v)
        {
            s.rejected.push_back(std::string(k.env) + "='" + trim(v) + "'");
        }

    }

    Settings load(const std::string& dllDir)
    {
        Settings s;
        s.logDir = dllDir;

        const KeyValues ini = readIni(paths::join(dllDir, kIniFileName));

        if (std::string v = resolve(ini, kLogLevel); !v.empty() && !logger::parseLevel(trim(v).c_str(), s.logLevel))
            recordReject(s, kLogLevel, v);
        if (std::string v = resolve(ini, kLogDir); !v.empty())
            s.logDir = v;
        if (std::string v = resolve(ini, kConsole); !v.empty() && !parseBool(v, s.console))
            recordReject(s, kConsole, v);
        if (std::string v = resolve(ini, kColor); !v.empty() && !parseBool(v, s.color))
            recordReject(s, kColor, v);
        if (std::string v = resolve(ini, kColorMode); !v.empty())
            s.colorMode = lower(trim(v));
        if (std::string v = resolve(ini, kCapture); !v.empty() && !parseBool(v, s.captureGameLog))
            recordReject(s, kCapture, v);
        if (std::string v = resolve(ini, kFaultIsolation); !v.empty() && !parseBool(v, s.faultIsolation))
            recordReject(s, kFaultIsolation, v);

        return s;
    }

    void dump(const Settings& s)
    {
        LOGC(Debug, kCategory, "log_level=%s console=%d color=%d color_mode=%s capture_game_log=%d fault_isolation=%d",
             logger::levelName(s.logLevel), s.console, s.color, s.colorMode.c_str(), s.captureGameLog, s.faultIsolation);
        LOGC(Debug, kCategory, "log_dir=%s", s.logDir.c_str());
        for (const std::string& r : s.rejected)
            LOGC(Warn, kCategory, "ignored unparseable config value: %s (kept the default)", r.c_str());
    }
}
