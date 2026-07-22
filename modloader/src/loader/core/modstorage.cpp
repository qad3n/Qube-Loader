#include "modloader/core/modstorage.h"
#include "core/log.h"
#include "core/paths.h"

#include <cstdio>
#include <fstream>
#include <map>

namespace modloader::modstorage
{
    namespace
    {
        constexpr char kCategory[] = "modstore";
        constexpr char kDirName[] = "data";
        constexpr char kBlobSuffix[] = ".bin";

        std::string g_dir; // <dllDir>/data
        std::map<std::string, std::string> g_scope; // sanitized DLL stem to sanitized scope ("" = root)

        // The directory holding a mod's blobs for its current scope: <data>/<stem>[/<scope>].
        std::string dirOf(const std::string& modStem)
        {
            std::string dir = paths::join(g_dir, paths::sanitizeComponent(modStem).c_str());
            const std::map<std::string, std::string>::const_iterator it =
                g_scope.find(paths::sanitizeComponent(modStem));
            if (it != g_scope.end() && !it->second.empty())
                dir = paths::join(dir, it->second.c_str());
            return dir;
        }

        std::string pathOf(const std::string& modStem, const std::string& key)
        {
            return paths::join(dirOf(modStem), (paths::sanitizeComponent(key) + kBlobSuffix).c_str());
        }
    }

    void init(const std::string& dllDir)
    {
        g_dir = paths::join(dllDir, kDirName);
        g_scope.clear();
        if (!paths::ensureDir(g_dir))
            LOGC(Warn, kCategory, "could not create data dir %s; mod save data will not persist", g_dir.c_str());
    }

    void setScope(const std::string& modStem, const std::string& scope)
    {
        g_scope[paths::sanitizeComponent(modStem)] = scope.empty() ? "" : paths::sanitizeComponent(scope);
    }

    int32_t get(const std::string& modStem, const std::string& key, void* out, int32_t size)
    {
        if (g_dir.empty())
            return 0;
        std::ifstream f(pathOf(modStem, key), std::ios::binary | std::ios::ate);
        if (!f.is_open())
            return 0;
        const std::streamoff end = f.tellg();
        if (end <= 0)
            return 0;
        const int32_t stored = static_cast<int32_t>(end);
        if (!out) // probe: report the blob size without reading
            return stored;
        if (size <= 0)
            return 0;
        const int32_t want = size < stored ? size : stored;
        f.seekg(0, std::ios::beg);
        f.read(static_cast<char*>(out), want);
        return static_cast<int32_t>(f.gcount());
    }

    bool put(const std::string& modStem, const std::string& key, const void* data, int32_t size)
    {
        if (g_dir.empty() || size < 0 || (size > 0 && !data))
            return false;
        if (!paths::ensureDir(dirOf(modStem)))
        {
            LOGC(Warn, kCategory, "could not create data dir for '%s'", modStem.c_str());
            return false;
        }
        std::ofstream f(pathOf(modStem, key), std::ios::binary | std::ios::trunc);
        if (!f.is_open())
        {
            LOGC(Warn, kCategory, "could not write blob '%s' for '%s'", key.c_str(), modStem.c_str());
            return false;
        }
        if (size > 0)
            f.write(static_cast<const char*>(data), size);
        return f.good();
    }

    bool remove(const std::string& modStem, const std::string& key)
    {
        if (g_dir.empty())
            return false;
        return std::remove(pathOf(modStem, key).c_str()) == 0;
    }

    bool has(const std::string& modStem, const std::string& key)
    {
        if (g_dir.empty())
            return false;
        std::ifstream f(pathOf(modStem, key), std::ios::binary);
        return f.is_open();
    }
}
