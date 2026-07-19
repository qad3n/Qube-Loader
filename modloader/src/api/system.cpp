#include "api/bridge.h"
#include "modloader/core/events.h"
#include "core/log.h"
#include "core/mem.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        void CUBE_CALL apiLogWrite(const CubeApi* api, CubeLogLevel level, const char* message)
        {
            if (!api || !message)
                return;
            logger::write(mapLevel(level), modName(api), "%s", message);
        }

        int32_t CUBE_CALL apiMemRead(const CubeApi* api, uint32_t address, void* out, uint32_t size)
        {
            if (!out || size == 0)
                return 0;
            if (!capabilityGate(api, CUBE_CAP_RAW_MEM, "mem.read"))
                return 0;
            const bool ok = mem::readBytes(static_cast<uintptr_t>(address), out, size);
            if (ok && size >= sizeof(uint32_t))
                LOGC(Trace, kApiCategory, "'%s' mem.read(0x%08X, %u) -> ok [0x%08X ...]",
                     modName(api), address, size, *static_cast<const uint32_t*>(out));
            else
                LOGC(Trace, kApiCategory, "'%s' mem.read(0x%08X, %u) -> %s",
                     modName(api), address, size, ok ? "ok" : "unreadable");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiMemReadable(const CubeApi* api, uint32_t address, uint32_t size)
        {
            if (!capabilityGate(api, CUBE_CAP_RAW_MEM, "mem.readable"))
                return 0;
            const void* src = reinterpret_cast<const void*>(static_cast<uintptr_t>(address));
            const bool ok = mem::readable(src, size);
            LOGC(Trace, kApiCategory, "'%s' mem.readable(0x%08X, %u) -> %d", modName(api), address, size, ok);
            return okInt(ok);
        }

        uint32_t CUBE_CALL apiMemRebase(const CubeApi* api, uint32_t staticAddress)
        {
            const uint32_t live = static_cast<uint32_t>(mem::rebase(staticAddress));
            LOGC(Trace, kApiCategory, "'%s' mem.rebase(0x%08X) -> 0x%08X", modName(api), staticAddress, live);
            return live;
        }

        int32_t CUBE_CALL apiMemWrite(const CubeApi* api, uint32_t address, const void* src, uint32_t size)
        {
            if (!src || size == 0)
                return 0;
            if (!capabilityGate(api, CUBE_CAP_RAW_MEM, "mem.write"))
                return 0;
            writeguard::Scope scope(api);
            const bool ok = mem::writeRaw(static_cast<uintptr_t>(address), src, size);
            if (size >= sizeof(uint32_t))
                LOGC(Debug, kApiCategory, "'%s' mem.write(0x%08X, %u) = [0x%08X ...] -> %s",
                     modName(api), address, size, *static_cast<const uint32_t*>(src), ok ? "ok" : "blocked");
            else
                LOGC(Debug, kApiCategory, "'%s' mem.write(0x%08X, %u) -> %s",
                     modName(api), address, size, ok ? "ok" : "blocked");
            return okInt(ok);
        }

        uint32_t CUBE_CALL apiEventsSubscribe(const CubeApi* api, CubeEvent event, CubeEventFn fn, void* user)
        {
            if (!api || !fn)
                return 0;
            const uint32_t token = modloader::events::subscribe(api, event, fn, user);
            LOGC(Debug, kApiCategory, "'%s' eventListener.subscribe(%s) -> token %u", modName(api),
                 modloader::events::eventName(event), token);
            return token;
        }

        int32_t CUBE_CALL apiEventsUnsubscribe(const CubeApi* api, uint32_t token)
        {
            const int32_t ok = modloader::events::unsubscribe(token);
            LOGC(Debug, kApiCategory, "'%s' eventListener.unsubscribe(token %u) -> %s", modName(api), token,
                 ok ? "ok" : "not found");
            return ok;
        }

    }

    void fillSystem(CubeApi& api)
    {
        api.log.write = &apiLogWrite;
        api.mem.read = &apiMemRead;
        api.mem.readable = &apiMemReadable;
        api.mem.rebase = &apiMemRebase;
        api.mem.write = &apiMemWrite;
        api.events.subscribe = &apiEventsSubscribe;
        api.events.unsubscribe = &apiEventsUnsubscribe;
    }

}
