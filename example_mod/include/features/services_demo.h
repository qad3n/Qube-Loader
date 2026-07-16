#pragma once
// Showcases the inter-mod ecosystem API (ABI 22) against THIS mod - a real ecosystem needs a second
// mod, but the round-trip is identical: publish a service, resolve it at onReady, receive directed
// messages, and send one. The menu (Mod tab) views the resulting state and drives the self-send.
#include "cube_mod.hpp"

namespace exmod
{
    // The shared contract both a provider and a consumer agree on (normally in a common header). The
    // registered service pointer is cast back to this by whoever resolves "example.ping".
    struct PingService
    {
        int (CUBE_CALL* ping)(int value);
    };

    class ServicesDemo
    {
    public:
        void install(cube::Mod& mod);
        // Send a ping message to this mod's own id; returns the handler's reply (or -1 if undelivered).
        int sendSelfPing(int value);

        bool serviceResolved() const { return m_resolved; }
        int lastPingResult() const { return m_lastPing; }
        unsigned messagesReceived() const { return m_received; }
        int lastReply() const { return m_lastReply; }
        int lastPayload() const { return m_lastPayload; }

    private:
        cube::Mod* m_mod = nullptr;
        bool m_resolved = false;
        int m_lastPing = 0;
        unsigned m_received = 0;
        int m_lastReply = 0;
        int m_lastPayload = 0;
    };

    ServicesDemo& servicesDemo();
}
