#include "modloader/core/conflict.h"
#include "core/log.h"

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <unordered_map>

namespace modloader::conflict
{
    namespace
    {
        constexpr char kCategory[] = "conflict";
        constexpr char kPrefixFormat[] = "CONFLICT: %s";
        constexpr int kMessageMax = 512;

        std::mutex g_throttleMutex;
        std::unordered_map<uint64_t, uint32_t> g_lastWarn; // conflict signature -> last-warned frame

        void emit(logger::Level level, const char* fmt, va_list args)
        {
            char body[kMessageMax];
            std::vsnprintf(body, sizeof(body), fmt, args);
            logger::write(level, kCategory, kPrefixFormat, body);
        }
    }

    bool throttle(uint64_t signature, uint32_t frame, uint32_t cooldownFrames)
    {
        std::lock_guard<std::mutex> lock(g_throttleMutex);
        std::unordered_map<uint64_t, uint32_t>::iterator it = g_lastWarn.find(signature);
        if (it != g_lastWarn.end() && frame - it->second < cooldownFrames)
            return false;

        g_lastWarn[signature] = frame;
        return true;
    }

    void warn(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        emit(logger::Level::Warn, fmt, args);
        va_end(args);
    }

    void error(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        emit(logger::Level::Error, fmt, args);
        va_end(args);
    }
}
