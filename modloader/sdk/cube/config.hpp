#pragma once
// Config: the mod-facing settings facade. Reads/writes this mod's own <dllDir>/config/<stem>.ini
// (the loader keys it by the mod's DLL stem); every call is a guarded one-liner returning the fallback
// if the loader is unavailable. Get with mod.config(); no offsets, no file I/O.

#include "cube/common.hpp"

#include <string>

namespace cube
{
    class Config
    {
    public:
        explicit Config(const CubeApi* api = nullptr) : m_api(api) {}

        int getInt(const char* key, int fallback = 0) const
        {
            return m_api ? m_api->config.getInt(m_api, key, fallback) : fallback;
        }

        float getFloat(const char* key, float fallback = 0.0f) const
        {
            return m_api ? static_cast<float>(m_api->config.getFloat(m_api, key, fallback)) : fallback;
        }

        bool getBool(const char* key, bool fallback = false) const
        {
            return m_api ? m_api->config.getBool(m_api, key, fallback ? 1 : 0) != 0 : fallback;
        }

        std::string getString(const char* key, const char* fallback = "") const
        {
            if (!m_api)
                return fallback ? fallback : "";
            char buffer[CUBE_CONFIG_STRING_MAX];
            m_api->config.getString(m_api, key, fallback, buffer, sizeof(buffer));
            return std::string(buffer);
        }

        bool setInt(const char* key, int value) const
        {
            return m_api && m_api->config.setInt(m_api, key, value) != 0;
        }

        bool setFloat(const char* key, float value) const
        {
            return m_api && m_api->config.setFloat(m_api, key, value) != 0;
        }

        bool setBool(const char* key, bool value) const
        {
            return m_api && m_api->config.setBool(m_api, key, value ? 1 : 0) != 0;
        }

        bool setString(const char* key, const char* value) const
        {
            return m_api && m_api->config.setString(m_api, key, value) != 0;
        }

    private:
        const CubeApi* m_api;
    };

}
