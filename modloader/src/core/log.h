#pragma once
// Thread-safe, color-coded logging to a console and cube_mod.log.

namespace logger
{
    enum class Level : int
    {
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
        Off
    };

    struct Options
    {
        Level minLevel = Level::Info;
        const char* logDir = nullptr;
        bool console = true;
        bool useColor = true;
        const char* colorMode = "detect";
    };

    void init(const Options& opts);
    void shutdown();
    void write(Level lvl, const char* category, const char* fmt, ...);
    const char* levelName(Level lvl);
    bool parseLevel(const char* text, Level& out);
}

#define LOGT(...) ::logger::write(::logger::Level::Trace, "mod", __VA_ARGS__)
#define LOGD(...) ::logger::write(::logger::Level::Debug, "mod", __VA_ARGS__)
#define LOGI(...) ::logger::write(::logger::Level::Info, "mod", __VA_ARGS__)
#define LOGW(...) ::logger::write(::logger::Level::Warn, "mod", __VA_ARGS__)
#define LOGE(...) ::logger::write(::logger::Level::Error, "mod", __VA_ARGS__)
#define LOGC(level, cat, ...) ::logger::write(::logger::Level::level, cat, __VA_ARGS__)