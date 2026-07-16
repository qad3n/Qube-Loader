#pragma once
// Services: the mod-facing inter-mod ecosystem facade. Publish/resolve a named shared service and
// exchange directed messages with another mod by its manifest id. Get it with mod.services(). No
// offsets. The service impl pointer and message payloads are raw and mod-owned: provider and consumer
// agree their layout by contract (a shared header), the loader never dereferences them.

#include "cube/common.hpp"

#include <functional>

namespace cube
{
    class Mod;

    // One received directed message, handed to an onMessage handler. reply() sets the value that the
    // sender's sendMessage returns; the payload is valid only for the duration of the call.
    class Message
    {
    public:
        explicit Message(CubeMessageArgs* args) : m_args(args) {}

        const char* sender() const { return m_args->senderId ? m_args->senderId : ""; }
        unsigned id() const { return m_args->msgId; }
        void* payload() const { return m_args->payload; }
        unsigned size() const { return m_args->payloadSize; }
        void reply(int result) { m_args->result = result; }

    private:
        CubeMessageArgs* m_args;
    };

    class Services
    {
    public:
        Services(const CubeApi* api, Mod* owner) : m_api(api), m_mod(owner) {}

        // Publish impl under name at version (>=1); re-registering the same name replaces it.
        bool registerService(const char* name, unsigned version, void* impl) const
        {
            return m_api && m_api->services.registerService(m_api, name, version, impl) != 0;
        }

        bool unregisterService(const char* name) const
        {
            return m_api && m_api->services.unregisterService(m_api, name) != 0;
        }

        // Resolve the highest-version provider of name with version >= minVersion (nullptr if none).
        void* query(const char* name, unsigned minVersion = 1, unsigned* outVersion = nullptr) const
        {
            return m_api ? m_api->services.query(m_api, name, minVersion, outVersion) : nullptr;
        }

        // Typed convenience: cast the resolved impl to the contract type both mods share.
        template <typename T>
        T* query(const char* name, unsigned minVersion = 1) const
        {
            return static_cast<T*>(query(name, minVersion, nullptr));
        }

        // Register a handler for messages addressed to this mod. Multiple handlers are allowed; each
        // receives every message. Defined out-of-line (routes through the Mod trampoline).
        void onMessage(std::function<void(Message&)> fn) const;

        // Send msgId + optional payload to the mod whose id == targetModId; returns the handler's reply
        // value, or -1 if the target id is unknown or has no message handler.
        int sendMessage(const char* targetModId, unsigned msgId, void* payload = nullptr, unsigned payloadSize = 0) const
        {
            return m_api ? m_api->services.sendMessage(m_api, targetModId, msgId, payload, payloadSize) : -1;
        }

    private:
        const CubeApi* m_api;
        Mod* m_mod;
    };
}
