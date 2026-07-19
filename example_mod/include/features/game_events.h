#pragma once
// Owns the mod's OBSERVE side: registers every game event listener on the loader's event bus and
// records each delivered event (per event counters, a rolling log, and an optional console echo).
// The Events tab is a pure view over this data; no event logic lives in the menu.

#include "cube_mod.hpp"

namespace exmod
{

    class GameEvents
    {
    public:
        static constexpr int kLogSize = 64;
        static constexpr int kLogLineLen = 80;

        GameEvents();

        // Register all game event handlers on the loader's listener (called once at init).
        void install(cube::EventListener& listener);
        // Count a delivered event, append it to the log, and echo to the console if enabled.
        void record(CubeEvent event);
        // Same, plus a short payload string (e.g. victim/damage) appended to the log line.
        void record(CubeEvent event, const char* detail);

        // Read API for the Events tab (indexed by CubeEvent value).
        unsigned countAt(int index) const;
        const char* nameAt(int index) const;
        bool& consoleEnabled(int index);

        int logLineCount() const { return m_count; }
        const char* logLineAt(int oldestFirst) const;
        void clearLog();

        // Representative EventListener::remove(Event) showcase: detach/reattach the AreaChange
        // listener at runtime (the loader stops/starts delivering it to this mod).
        bool areaChangeListening() const { return m_areaChangeListening; }
        void setAreaChangeListening(bool listening);

    private:
        void pushLine(const char* name, const char* detail);

        cube::EventListener* m_listener = nullptr;
        unsigned m_counts[CUBE_EVENT_COUNT] = {};
        bool m_console[CUBE_EVENT_COUNT] = {};
        char m_lines[kLogSize][kLogLineLen] = {};
        int m_head = 0;
        int m_count = 0;
        unsigned m_seq = 0;
        bool m_areaChangeListening = true;
    };

    GameEvents& gameEvents();

}
