#pragma once
// Consumes the headless example_lib companion mod to exercise the whole inter-mod ecosystem API:
// resolve its service at READY, send it directed messages, and (for the register/unregister half)
// publish a marker service of this mod's own. The Mod > Services tab views this and drives the sends.
#include "cube_mod.hpp"

namespace exmod
{
    class ServicesDemo
    {
    public:
        void install(cube::Mod& mod);
        // Send a ping to example_lib; returns its reply (payload + 1), or -1 if undelivered.
        int sendLibPing(int value);
        // Publish or drop this mod's own marker service, so the tab can show register/unregister + query.
        void setSelfServiceRegistered(bool registered);
        bool selfServiceRegistered() const;

        bool libResolved() const { return m_libResolved; }
        int lastPing() const { return m_lastPing; }

    private:
        cube::Mod* m_mod = nullptr;
        bool m_libResolved = false;
        int m_lastPing = 0;
    };

    ServicesDemo& servicesDemo();
}
