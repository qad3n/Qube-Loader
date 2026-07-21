#include "core/log.h"
#include "core/paths.h"

#include <windows.h>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

namespace logger
{
    namespace
    {
        constexpr char kLogFileName[] = "cube_mod.log";
        constexpr char kConsoleTitle[] = "Cube World - Debug Mod";
        constexpr char kConsoleDevice[] = "CONOUT$";
        constexpr int kMaxMessageLen = 2048;
        constexpr int kTimestampLen = 32;
        constexpr int kHoursPerHalfDay = 12;
        constexpr int kYearModulo = 100;

        #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
        #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
        #endif
        #ifndef ENABLE_EXTENDED_FLAGS
        #define ENABLE_EXTENDED_FLAGS 0x0080
        #endif
        #ifndef ENABLE_QUICK_EDIT_MODE
        #define ENABLE_QUICK_EDIT_MODE 0x0040
        #endif

        // Two coloring methods because neither works everywhere. wine: an AllocConsole wineconsole
        // honors SetConsoleTextAttribute but prints ANSI literally; a Unix pty is the reverse.
        enum class ColorMode
        {
            None,
            Ansi,
            Win32
        };

        struct Color
        {
            const char* ansi;
            WORD attr;
        };

        constexpr Color kColorGrey = {"\x1b[90m", FOREGROUND_INTENSITY};
        constexpr Color kColorBlue = {"\x1b[94m", FOREGROUND_BLUE | FOREGROUND_INTENSITY};
        constexpr Color kColorYellow = {"\x1b[93m", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY};
        constexpr Color kColorRed = {"\x1b[91m", FOREGROUND_RED | FOREGROUND_INTENSITY};
        constexpr Color kColorWhite = {"\x1b[97m", FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY};
        constexpr Color kColorTimestamp = kColorGrey;
        constexpr char kAnsiReset[] = "\x1b[0m";
        constexpr WORD kAttrDefault = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

        struct LevelInfo
        {
            Level level;
            const char* tag;
            const char* name;
            Color color;
        };

        constexpr LevelInfo kLevels[] =
        {
            {Level::Trace, "TRACE", "trace", kColorGrey},
            {Level::Debug, "DEBUG", "debug", kColorGrey},
            {Level::Info, "INFO", "info", kColorBlue},
            {Level::Warn, "WARN", "warn", kColorYellow},
            {Level::Error, "ERROR", "error", kColorRed},
            {Level::Off, "OFF", "off", kColorGrey},
        };

        // recursive: the crash filter may log from a thread already holding this lock.
        std::recursive_mutex g_mutex;
        FILE* g_console = nullptr;
        FILE* g_file = nullptr;
        HANDLE g_conHandle = INVALID_HANDLE_VALUE;
        WORD g_baseAttr = kAttrDefault;
        ColorMode g_colorMode = ColorMode::None;
        bool g_allocatedConsole = false;
        bool g_ready = false;
        bool g_color = true;
        Level g_minLvl = Level::Info;

        const LevelInfo& levelInfo(Level lvl)
        {
            for (const LevelInfo& li : kLevels)
            {
                if (li.level == lvl)
                    return li;
            }
            return kLevels[static_cast<int>(Level::Info)];
        }

        // Enable QuickEdit mouse selection. wine: already on but its conhost lacks click-drag
        // selection, so this only helps real Windows; under Wine read the log file in a native terminal.
        void enableConsoleSelection()
        {
            HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
            DWORD inMode = 0;

            if (in == INVALID_HANDLE_VALUE || !GetConsoleMode(in, &inMode))
                return;

            SetConsoleMode(in, inMode | ENABLE_EXTENDED_FLAGS | ENABLE_QUICK_EDIT_MODE);
        }

        // Enable VT/ANSI processing and confirm the bit stuck. wine 11 conhost + Windows 10+ support
        // it (ANSI renders); older Wine/Windows do not, and there we fall back to the Win32 API.
        bool tryEnableVt(HANDLE h)
        {
            DWORD mode = 0;

            if (h == INVALID_HANDLE_VALUE || !GetConsoleMode(h, &mode))
                return false;
            if (!SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
                return false;

            DWORD after = 0;
            return GetConsoleMode(h, &after) && (after & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
        }

        void timestamp(char* buf, size_t n)
        {
            SYSTEMTIME t;
            GetLocalTime(&t);
            int hour12 = t.wHour % kHoursPerHalfDay;

            if (hour12 == 0)
                hour12 = kHoursPerHalfDay;

            const char* ampm = t.wHour < kHoursPerHalfDay ? "am" : "pm";
            snprintf(buf, n, "%d/%d/%02d %d:%02d%s", t.wMonth, t.wDay, t.wYear % kYearModulo, hour12, t.wMinute, ampm);
        }

        // Win32 mode must write via WriteConsoleA on the SAME handle it set the attribute on (the CRT
        // stdout stream is a different handle after freopen and loses the color); ANSI/None use stdout.
        void consoleWrite(const char* text)
        {
            if (g_colorMode == ColorMode::Win32)
            {
                if (g_conHandle == INVALID_HANDLE_VALUE)
                    return;

                DWORD written = 0;
                WriteConsoleA(g_conHandle, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
                return;
            }

            fputs(text, g_console);
            fflush(g_console);
        }

        void writeSegment(const Color& color, const char* fmt, const char* value)
        {
            char buf[kMaxMessageLen];
            snprintf(buf, sizeof(buf), fmt, value);

            switch (g_colorMode)
            {
                case ColorMode::Win32:
                    SetConsoleTextAttribute(g_conHandle, color.attr);
                    consoleWrite(buf);
                    break;
                case ColorMode::Ansi:
                    fputs(color.ansi, g_console);
                    fputs(buf, g_console);
                    fputs(kAnsiReset, g_console);
                    fflush(g_console);
                    break;
                case ColorMode::None:
                    consoleWrite(buf);
                    break;
            }
        }

        // Info/debug/trace messages read white; warn/error keep the whole line in their color.
        const Color& messageColor(const LevelInfo& li)
        {
            if (li.level == Level::Warn || li.level == Level::Error)
                return li.color;

            return kColorWhite;
        }

        void writeConsole(const LevelInfo& li, const char* ts, const char* category, const char* msg)
        {
            writeSegment(kColorTimestamp, "[%s] ", ts);
            writeSegment(li.color, "[%s] ", li.tag);
            writeSegment(li.color, "[%s]: ", category);
            writeSegment(messageColor(li), "%s\n", msg);

            if (g_colorMode == ColorMode::Win32)
                SetConsoleTextAttribute(g_conHandle, g_baseAttr);
        }

        void colorCheck()
        {
            if (!g_console || g_colorMode == ColorMode::None)
                return;

            consoleWrite("color check: ");

            for (const LevelInfo& li : kLevels)
            {
                if (li.level == Level::Off)
                    continue;
                writeSegment(li.color, "%s ", li.tag);
            }

            if (g_colorMode == ColorMode::Win32)
                SetConsoleTextAttribute(g_conHandle, g_baseAttr);

            consoleWrite("\n");
        }

        void emit(Level lvl, const char* category, const char* msg)
        {
            if (static_cast<int>(lvl) < static_cast<int>(g_minLvl))
                return;

            char ts[kTimestampLen];
            timestamp(ts, sizeof(ts));
            const LevelInfo& li = levelInfo(lvl);

            if (g_console)
                writeConsole(li, ts, category, msg);

            if (g_file)
            {
                fprintf(g_file, "[%s] [%s] [%s]: %s\n", ts, li.tag, category, msg);
                fflush(g_file);
            }
        }
    }

    const char* levelName(Level lvl)
    {
        return levelInfo(lvl).name;
    }

    bool parseLevel(const char* text, Level& out)
    {
        std::string s = text ? text : "";
        for (char& c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        for (const LevelInfo& li : kLevels)
        {
            if (s == li.name)
            {
                out = li.level;
                return true;
            }
        }

        return false;
    }

    void init(const Options& opts)
    {
        std::lock_guard<std::recursive_mutex> lk(g_mutex);
        if (g_ready)
            return;

        g_color = opts.useColor;
        g_minLvl = opts.minLevel;

        const char* consoleSource = "none";
        if (opts.console)
        {
            // Console handle: inherited char stdout, else an allocated one for a GUI exe (Cube.exe).
            // wine reports VT enabled but prints ANSI literally, so method is chosen by path: allocated
            // -> Win32 attrs, inherited pty -> ANSI, redirected -> plain. Override with color_mode.
            HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
            const DWORD fileType = (h && h != INVALID_HANDLE_VALUE) ? GetFileType(h) : FILE_TYPE_UNKNOWN;
            bool haveConsole = false;

            if (fileType == FILE_TYPE_CHAR)
            {
                g_console = stdout;
                g_conHandle = h;
                consoleSource = "inherited";
                haveConsole = true;
            }
            else if (fileType == FILE_TYPE_UNKNOWN)
            {
                static_cast<void>(AllocConsole()); // may already be attached; the freopen below is the real guard
                SetConsoleTitleA(kConsoleTitle);
                // Keep the console visible but make it a non-activating TOOL window: this drops it from
                // the alt-tab/taskbar list and stops it ever taking foreground, so it cannot minimize an
                // exclusive-fullscreen game (device lost) nor block alt-tab back into it. The game window
                // stays the sole foreground/alt-tab target and recovers its device cleanly.
                if (HWND con = GetConsoleWindow())
                {
                    LONG_PTR ex = GetWindowLongPtrA(con, GWL_EXSTYLE);
                    ex = (ex | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW) & ~static_cast<LONG_PTR>(WS_EX_APPWINDOW);
                    ShowWindow(con, SW_HIDE); // hide so the tool-window/taskbar style change takes effect
                    SetWindowLongPtrA(con, GWL_EXSTYLE, ex);
                    ShowWindow(con, SW_SHOWNOACTIVATE);
                }
                if (freopen(kConsoleDevice, "w", stdout) != nullptr)
                    g_console = stdout;
                static_cast<void>(freopen(kConsoleDevice, "w", stderr));
                g_conHandle = GetStdHandle(STD_OUTPUT_HANDLE);
                g_allocatedConsole = true;
                consoleSource = "allocated";
                haveConsole = true;
                enableConsoleSelection();
            }
            else
            {
                g_console = stdout;
                consoleSource = "redirected";
            }

            if (haveConsole)
            {
                const char* forced = opts.colorMode ? opts.colorMode : "detect";
                if (_stricmp(forced, "ansi") == 0)
                {
                    tryEnableVt(g_conHandle);
                    g_colorMode = ColorMode::Ansi;
                }
                else if (_stricmp(forced, "win32") == 0)
                    g_colorMode = ColorMode::Win32;
                else if (_stricmp(forced, "none") == 0)
                    g_colorMode = ColorMode::None;
                else if (g_allocatedConsole)
                    g_colorMode = ColorMode::Win32;
                else
                {
                    tryEnableVt(g_conHandle);
                    g_colorMode = ColorMode::Ansi;
                }

                if (g_colorMode == ColorMode::Win32)
                {
                    CONSOLE_SCREEN_BUFFER_INFO info{};
                    if (g_conHandle != INVALID_HANDLE_VALUE &&
                        GetConsoleScreenBufferInfo(g_conHandle, &info))
                        g_baseAttr = info.wAttributes;
                }
            }

            if (!g_color)
                g_colorMode = ColorMode::None;
        }

        const std::string path = paths::join(opts.logDir ? opts.logDir : ".", kLogFileName);
        g_file = fopen(path.c_str(), "w");
        g_ready = true;

        const char* colorModeName = "none";
        switch (g_colorMode)
        {
            case ColorMode::Ansi:
                colorModeName = "ansi";
                break;
            case ColorMode::Win32:
                colorModeName = "win32";
                break;
            case ColorMode::None:
                break;
        }

        char msg[kMaxMessageLen];
        snprintf(msg, sizeof(msg), "logging up | level=%s | console=%s | color=%s | file=%s",
                 levelName(g_minLvl), consoleSource, colorModeName, path.c_str());

        emit(Level::Debug, "log", msg);
        if (!g_file)
            emit(Level::Warn, "log", "could not open log file for writing");

        colorCheck();
    }

    void shutdown()
    {
        std::lock_guard<std::recursive_mutex> lk(g_mutex);
        if (!g_ready)
            return;

        if (g_file)
        {
            fclose(g_file);
            g_file = nullptr;
        }
        if (g_console)
        {
            if (g_allocatedConsole)
                FreeConsole();

            g_console = nullptr;
            g_allocatedConsole = false;
            g_conHandle = INVALID_HANDLE_VALUE;
            g_colorMode = ColorMode::None;
        }
        g_ready = false;
    }

    void write(Level lvl, const char* category, const char* fmt, ...)
    {
        std::lock_guard<std::recursive_mutex> lk(g_mutex);
        if (!g_ready)
            return;

        char msg[kMaxMessageLen];

        va_list ap;
        va_start(ap, fmt);
        vsnprintf(msg, sizeof(msg), fmt, ap);
        va_end(ap);

        emit(lvl, category, msg);
    }

}
