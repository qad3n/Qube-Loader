#include "features/services_demo.h"
#include "mod_context.h"

namespace exmod
{
    namespace
    {
        constexpr char kServiceName[] = "example.ping";
        constexpr char kSelfId[] = "cube_mod.example"; // this mod's manifest id (see example_mod.cpp)
        constexpr unsigned kPingMsg = 1;

        // The published service implementation (the contract's ping()): doubles its argument.
        int CUBE_CALL pingImpl(int value)
        {
            return value * 2;
        }

        PingService g_ping = { &pingImpl };
    }

    ServicesDemo& servicesDemo()
    {
        static ServicesDemo g_demo;
        return g_demo;
    }

    void ServicesDemo::install(cube::Mod& mod)
    {
        m_mod = &mod;

        // Publish a shared service any other mod can resolve by name at its onReady.
        mod.services().registerService(kServiceName, 1, &g_ping);

        // Receive directed messages addressed to this mod's id: reply with payload + 1 so the sender
        // sees a concrete round-trip value.
        mod.services().onMessage([this](cube::Message& msg)
        {
            if (msg.id() != kPingMsg || msg.size() < sizeof(int))
                return;
            m_lastPayload = *static_cast<const int*>(msg.payload());
            m_lastReply = m_lastPayload + 1;
            ++m_received;
            msg.reply(m_lastReply);
            cubeLogf(g_api, CUBE_LOG_INFO, "example_mod: message %u from '%s' payload=%d -> reply %d",
                     msg.id(), msg.sender(), m_lastPayload, m_lastReply);
        });

        // Resolve peers only at READY (every mod has loaded + passed dependency resolution by then).
        mod.eventListener.onReady([this]
        {
            PingService* svc = m_mod->services().query<PingService>(kServiceName, 1);
            m_resolved = svc != nullptr;
            if (svc && svc->ping)
            {
                m_lastPing = svc->ping(21);
                cubeLogf(g_api, CUBE_LOG_INFO, "example_mod: resolved '%s' at READY; ping(21) -> %d",
                         kServiceName, m_lastPing);
            }
        });
    }

    int ServicesDemo::sendSelfPing(int value)
    {
        if (!m_mod)
            return -1;
        int payload = value;
        return m_mod->services().sendMessage(kSelfId, kPingMsg, &payload, sizeof(payload));
    }
}
