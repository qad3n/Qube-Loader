#include "api/bridge.h"
#include "overlay/overlay.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        uint32_t CUBE_CALL apiOverlayRegisterMenu(const CubeApi* api, CubeOverlayDrawFn fn, void* user,
                                                  uint32_t toggleKey, int32_t startOpen)
        {
            if (!capabilityGate(api, CUBE_CAP_OVERLAY, "overlay.registerMenu"))
                return 0;
            const uint32_t handle = overlay::registerMenu(api, fn, user, toggleKey, startOpen != 0);
            LOGC(Debug, kApiCategory, "'%s' overlay.registerMenu(key=%u, open=%d) -> %u",
                 modName(api), toggleKey, startOpen, handle);
            return handle;
        }

        int32_t CUBE_CALL apiOverlayUnregisterMenu(const CubeApi* api, uint32_t handle)
        {
            const bool ok = overlay::unregisterMenu(api, handle);
            LOGC(Debug, kApiCategory, "'%s' overlay.unregisterMenu(%u) -> %s", modName(api), handle, ok ? "ok" : "none");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiOverlaySetVisible(const CubeApi* api, uint32_t handle, int32_t visible)
        {
            if (!capabilityGate(api, CUBE_CAP_OVERLAY, "overlay.setVisible"))
                return 0;
            return okInt(overlay::setVisible(handle, visible != 0));
        }

        int32_t CUBE_CALL apiOverlayIsVisible(const CubeApi*, uint32_t handle)
        {
            return okInt(overlay::isVisible(handle));
        }

        int32_t CUBE_CALL apiOverlaySetToggleKey(const CubeApi* api, uint32_t handle, uint32_t vkey)
        {
            if (!capabilityGate(api, CUBE_CAP_OVERLAY, "overlay.setToggleKey"))
                return 0;
            return okInt(overlay::setToggleKey(handle, vkey));
        }

        int32_t CUBE_CALL apiOverlaySetPassthrough(const CubeApi* api, uint32_t handle, int32_t passthrough)
        {
            if (!capabilityGate(api, CUBE_CAP_OVERLAY, "overlay.setPassthrough"))
                return 0;
            return okInt(overlay::setPassthrough(handle, passthrough != 0));
        }

        int32_t CUBE_CALL apiOverlayPassthrough(const CubeApi*, uint32_t handle)
        {
            return okInt(overlay::passthrough(handle));
        }

        int32_t CUBE_CALL apiOverlaySetUiScale(const CubeApi* api, float scale)
        {
            if (!capabilityGate(api, CUBE_CAP_OVERLAY, "overlay.setUiScale"))
                return 0;
            overlay::setUiScale(scale);
            return 1;
        }

        float CUBE_CALL apiOverlayUiScale(const CubeApi*)
        {
            return overlay::uiScale();
        }

        float CUBE_CALL apiOverlayDpiScale(const CubeApi*)
        {
            return overlay::dpiScale();
        }

        void* CUBE_CALL apiOverlayContext(const CubeApi*)
        {
            return overlay::context();
        }

        void CUBE_CALL apiOverlayAllocFuncs(const CubeApi*, void** allocFn, void** freeFn, void** userData)
        {
            overlay::allocFuncs(allocFn, freeFn, userData);
        }
    }

    void fillOverlay(CubeApi& api)
    {
        api.overlay.registerMenu = &apiOverlayRegisterMenu;
        api.overlay.unregisterMenu = &apiOverlayUnregisterMenu;
        api.overlay.setVisible = &apiOverlaySetVisible;
        api.overlay.isVisible = &apiOverlayIsVisible;
        api.overlay.setToggleKey = &apiOverlaySetToggleKey;
        api.overlay.setPassthrough = &apiOverlaySetPassthrough;
        api.overlay.passthrough = &apiOverlayPassthrough;
        api.overlay.setUiScale = &apiOverlaySetUiScale;
        api.overlay.uiScale = &apiOverlayUiScale;
        api.overlay.dpiScale = &apiOverlayDpiScale;
        api.overlay.context = &apiOverlayContext;
        api.overlay.allocFuncs = &apiOverlayAllocFuncs;
    }
}
