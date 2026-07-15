#include "hooks/detour.h"
#include "core/log.h"
#include "util/fmt.h"

#include "MinHook.h"

#include <atomic>
#include <cstdint>

namespace hooks::detour
{
    namespace
    {
        constexpr char kCategory[] = "detour";
        std::atomic<bool> g_initialized{false};
    }

    bool init()
    {
        if (g_initialized.load())
            return true;
        const MH_STATUS status = MH_Initialize();

        if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
        {
            LOGC(Error, kCategory, "MH_Initialize failed: %s", MH_StatusToString(status));
            return false;
        }

        g_initialized.store(true);
        LOGC(Debug, kCategory, "MinHook initialized");
        return true;
    }

    void shutdown()
    {
        if (!g_initialized.load())
            return;

        MH_Uninitialize();
        g_initialized.store(false);
        LOGC(Debug, kCategory, "MinHook uninitialized");
    }

    bool create(void* target, void* detourFn, void** original)
    {
        if (!target || !detourFn || !original || !init())
            return false;

        MH_STATUS status = MH_CreateHook(target, detourFn, original);
        if (status != MH_OK)
        {
            LOGC(Error, kCategory, "MH_CreateHook(0x%X) failed: %s", fmt::ptr(target), MH_StatusToString(status));
            return false;
        }

        status = MH_EnableHook(target);
        if (status != MH_OK)
        {
            LOGC(Error, kCategory, "MH_EnableHook(0x%X) failed: %s", fmt::ptr(target), MH_StatusToString(status));
            MH_RemoveHook(target);
            return false;
        }

        LOGC(Debug, kCategory, "hooked 0x%X", fmt::ptr(target));
        return true;
    }

    bool remove(void* target)
    {
        if (!target)
            return false;

        MH_STATUS status = MH_DisableHook(target);
        if (status != MH_OK && status != MH_ERROR_NOT_CREATED)
            LOGC(Warn, kCategory, "MH_DisableHook(0x%X) failed: %s", fmt::ptr(target), MH_StatusToString(status));

        status = MH_RemoveHook(target);
        if (status != MH_OK && status != MH_ERROR_NOT_CREATED)
        {
            LOGC(Warn, kCategory, "MH_RemoveHook(0x%X) failed: %s", fmt::ptr(target), MH_StatusToString(status));
            return false;
        }
        return true;
    }
}