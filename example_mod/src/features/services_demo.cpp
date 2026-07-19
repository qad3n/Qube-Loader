#include "features/services_demo.h"
#include "mod_context.h"
#include "example_lib_api.h"

namespace exmod
{
    namespace
    {
        constexpr char kSelfService[] = "example.consumer";
        constexpr int kPingProbe = 21; // pinged at READY to prove the round-trip
        int g_selfMarker = 0; // the self service impl pointer (its presence is the whole contract)
    }

    ServicesDemo& servicesDemo()
    {
        static ServicesDemo g_demo;
        return g_demo;
    }

    void ServicesDemo::install(cube::Mod& mod)
    {
        m_mod = &mod;
        mod.services().registerService(kSelfService, 1, &g_selfMarker);

        // Resolve peers only at READY (every mod has loaded + passed dependency resolution by then).
        mod.eventListener.onReady([this]
        {
            exlib::PingService* svc = m_mod->services().query<exlib::PingService>(exlib::kServiceName, exlib::kServiceVersion);
            m_libResolved = svc != nullptr;
            if (svc && svc->ping)
            {
                m_lastPing = svc->ping(kPingProbe);
                cubeLogf(g_api, CUBE_LOG_INFO, "example_mod: resolved example_lib; ping(%d) -> %d", kPingProbe, m_lastPing);
            }
        });
    }

    int ServicesDemo::sendLibPing(int value)
    {
        if (!m_mod)
            return -1;
        int payload = value;
        return m_mod->services().sendMessage(exlib::kModId, exlib::kPingMessage, &payload, sizeof(payload));
    }

    void ServicesDemo::setSelfServiceRegistered(bool registered)
    {
        if (!m_mod)
            return;
        if (registered)
            m_mod->services().registerService(kSelfService, 1, &g_selfMarker);
        else
            m_mod->services().unregisterService(kSelfService);
    }

    bool ServicesDemo::selfServiceRegistered() const
    {
        return m_mod && m_mod->services().query(kSelfService, 1) != nullptr;
    }
}
