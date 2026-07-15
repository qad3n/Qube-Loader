#pragma once
// Logger: the mod-facing logging facade.

#include "cube/common.hpp"

namespace cube
{
    class Logger
    {
    public:
        explicit Logger(const CubeApi* api = nullptr) : m_api(api) {}

        void trace(const char* fmt, ...) const
        {
            va_list a;
            va_start(a, fmt);
            write(CUBE_LOG_TRACE, fmt, a);
            va_end(a);
        }

        void debug(const char* fmt, ...) const
        {
            va_list a;
            va_start(a, fmt);
            write(CUBE_LOG_DEBUG, fmt, a);
            va_end(a);
        }

        void info(const char* fmt, ...) const
        {
            va_list a;
            va_start(a, fmt);
            write(CUBE_LOG_INFO, fmt, a);
            va_end(a);
        }

        void warn(const char* fmt, ...) const
        {
            va_list a;
            va_start(a, fmt);
            write(CUBE_LOG_WARN, fmt, a);
            va_end(a);
        }

        void error(const char* fmt, ...) const
        {
            va_list a;
            va_start(a, fmt);
            write(CUBE_LOG_ERROR, fmt, a);
            va_end(a);
        }

    private:
        void write(CubeLogLevel level, const char* fmt, va_list args) const
        {
            if (!m_api)
                return;
            char buffer[CUBE_LOG_BUFFER];
            vsnprintf(buffer, sizeof(buffer), fmt, args);
            m_api->log.write(m_api, level, buffer);
        }

        const CubeApi* m_api;
    };

}
