#include "core/config.h"
#include "core/ini.h"
#include "core/log.h"
#include "core/paths.h"

#include <windows.h>
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
        constexpr Key kOverlay = {"CUBE_MOD_OVERLAY", "overlay"};

        std::string env(const char* name)
        {
            char buf[kEnvBufSize];
            const DWORD n = GetEnvironmentVariableA(name, buf, sizeof(buf));
            if (n > 0 && n < static_cast<DWORD>(sizeof(buf)))
                return std::string(buf, n);
            return std::string();
        }

        std::string resolve(const ini::KeyValues& kv, const Key& k)
        {
            const std::string fromEnv = env(k.env);
            if (!fromEnv.empty())
                return fromEnv;
            const ini::KeyValues::const_iterator it = kv.find(k.ini);
            if (it == kv.end())
                return std::string();
            return it->second;
        }

        void recordReject(Settings& s, const Key& k, const std::string& v)
        {
            s.rejected.push_back(std::string(k.env) + "='" + ini::trim(v) + "'");
        }

    }

    Settings load(const std::string& dllDir)
    {
        Settings s;
        s.logDir = dllDir;

        const ini::KeyValues kv = ini::read(paths::join(dllDir, kIniFileName));

        if (std::string v = resolve(kv, kLogLevel); !v.empty() && !logger::parseLevel(ini::trim(v).c_str(), s.logLevel))
            recordReject(s, kLogLevel, v);
        if (std::string v = resolve(kv, kLogDir); !v.empty())
            s.logDir = v;
        if (std::string v = resolve(kv, kConsole); !v.empty() && !ini::parseBool(v, s.console))
            recordReject(s, kConsole, v);
        if (std::string v = resolve(kv, kColor); !v.empty() && !ini::parseBool(v, s.color))
            recordReject(s, kColor, v);
        if (std::string v = resolve(kv, kColorMode); !v.empty())
            s.colorMode = ini::lower(ini::trim(v));
        if (std::string v = resolve(kv, kCapture); !v.empty() && !ini::parseBool(v, s.captureGameLog))
            recordReject(s, kCapture, v);
        if (std::string v = resolve(kv, kFaultIsolation); !v.empty() && !ini::parseBool(v, s.faultIsolation))
            recordReject(s, kFaultIsolation, v);
        if (std::string v = resolve(kv, kOverlay); !v.empty() && !ini::parseBool(v, s.overlay))
            recordReject(s, kOverlay, v);

        return s;
    }

    void dump(const Settings& s)
    {
        LOGC(Debug, kCategory, "log_level=%s console=%d color=%d color_mode=%s capture_game_log=%d fault_isolation=%d overlay=%d",
             logger::levelName(s.logLevel), s.console, s.color, s.colorMode.c_str(), s.captureGameLog, s.faultIsolation, s.overlay);
        LOGC(Debug, kCategory, "log_dir=%s", s.logDir.c_str());
        for (const std::string& r : s.rejected)
            LOGC(Warn, kCategory, "ignored unparseable config value: %s (kept the default)", r.c_str());
    }
}
