#include "api/bridge.h"
#include "game/gamehooks/gamehooks.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiHooksOn(const CubeApi* api, CubeHook hook, CubeHookFn fn, void* user)
        {
            if (!api || !fn)
                return 0;
            game::gamehooks::subscribe(api, hook, fn, user);
            LOGC(Debug, kApiCategory, "'%s' eventHook.on(%s)", modName(api), game::gamehooks::hookName(hook));
            return 1;
        }

        int32_t CUBE_CALL apiHooksOff(const CubeApi* api, CubeHook hook)
        {
            if (!api)
                return 0;
            const int32_t removed = game::gamehooks::unsubscribeHook(api, hook);
            LOGC(Debug, kApiCategory, "'%s' eventHook.off(%s) -> %d removed", modName(api),
                 game::gamehooks::hookName(hook), removed);
            return okInt(removed > 0);
        }

        int32_t CUBE_CALL apiHooksOnRaw(const CubeApi* api, uint32_t address, CubeCallConv cc,
                                        int32_t argCount, CubeHookFn fn, void* user)
        {
            if (!api || !fn)
                return 0;
            const int32_t ok = game::gamehooks::installRaw(api, address, cc, argCount, fn, user);
            LOGC(Debug, kApiCategory, "'%s' eventHook.raw(0x%08X, cc %d, %d args) -> %s", modName(api),
                 address, static_cast<int>(cc), argCount, ok ? "ok" : "fail");
            return ok;
        }

        int32_t CUBE_CALL apiHooksInstallRawDetour(const CubeApi* api, uint32_t address, void* detour,
                                                   void** trampoline)
        {
            if (!api)
                return 0;
            const int32_t ok = game::gamehooks::installRawDetour(api, address, detour, trampoline);
            LOGC(Debug, kApiCategory, "'%s' eventHook.rawDetour(0x%08X) -> %s", modName(api), address,
                 ok ? "ok" : "fail");
            return ok;
        }

        int32_t CUBE_CALL apiHooksRemoveRaw(const CubeApi* api, uint32_t address)
        {
            if (!api)
                return 0;
            const int32_t ok = game::gamehooks::removeRaw(api, address);
            LOGC(Debug, kApiCategory, "'%s' eventHook.removeRaw(0x%08X) -> %s", modName(api), address,
                 ok ? "ok" : "fail");
            return ok;
        }

        uint32_t CUBE_CALL apiHooksBuiltinTarget(const CubeApi* api, int32_t hook)
        {
            if (!api)
                return 0;
            const CubeHook h = static_cast<CubeHook>(hook);
            const uint32_t addr = game::gamehooks::builtinTarget(h);
            LOGC(Debug, kApiCategory, "'%s' eventHook.builtinTarget(%s) -> 0x%08X", modName(api),
                 game::gamehooks::hookName(h), addr);
            return addr;
        }
    }

    void fillHooks(CubeApi& api)
    {
        api.hooks.on = &apiHooksOn;
        api.hooks.off = &apiHooksOff;
        api.hooks.onRaw = &apiHooksOnRaw;
        api.hooks.installRawDetour = &apiHooksInstallRawDetour;
        api.hooks.removeRaw = &apiHooksRemoveRaw;
        api.hooks.builtinTarget = &apiHooksBuiltinTarget;
    }
}
