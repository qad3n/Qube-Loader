#pragma once
// Shared internals for the api bridge: each api/<domain>.cpp exposes one fillX(CubeApi&).
#include "modloader/core/owner_name.h"
#include "modloader/core/writeguard.h"
#include "core/log.h"
#include "cube_sdk.h"

#include <cstdio>

namespace modloader::api
{

    constexpr char kApiCategory[] = "api"; // every mod->loader call is logged here

    // Recovers the calling mod's name from its CubeApi (for the per-call log line).
    inline const char* modName(const CubeApi* api)
    {
        return modloader::ownerName(api);
    }

    inline logger::Level mapLevel(CubeLogLevel level)
    {
        switch (level)
        {
            case CUBE_LOG_TRACE:
                return logger::Level::Trace;
            case CUBE_LOG_DEBUG:
                return logger::Level::Debug;
            case CUBE_LOG_INFO:
                return logger::Level::Info;
            case CUBE_LOG_WARN:
                return logger::Level::Warn;
            case CUBE_LOG_ERROR:
            default:
                return logger::Level::Error;
        }
    }

    inline int32_t okInt(bool ok)
    {
        return ok ? 1 : 0;
    }

    // "reader get" reducer; `absent` is the log word on read failure ("unavailable"/"none").
    // `addr` (optional) names the struct's live-base field so a successful read logs the resolved
    // memory address alongside "ok" - the single most useful datum for a mod dev inspecting a read.
    template <typename T>
    int32_t bridgeGet(const CubeApi* api, T* out, bool (*read)(T&), const char* label, const char* absent, uint32_t T::* addr = nullptr)
    {
        if (!out)
            return 0;
        T value = {};
        value.structSize = sizeof(T);
        const bool ok = read(value);
        if (ok && addr)
            LOGC(Trace, kApiCategory, "'%s' %s -> ok (0x%08X)", modName(api), label, value.*addr);
        else
            LOGC(Trace, kApiCategory, "'%s' %s -> %s", modName(api), label, ok ? "ok" : absent);
        if (!ok)
            return 0;
        *out = value;
        return 1;
    }

    // "list" reducer: log the "(max %d) -> %d" line and return the count.
    inline int32_t bridgeList(const CubeApi* api, const char* label, int32_t maxCount, int32_t count)
    {
        LOGC(Trace, kApiCategory, "'%s' %s(max %d) -> %d", modName(api), label, maxCount, count);
        return count;
    }

    // Same, for lists of live objects: also prints each entry's base address (capped) so a mod dev
    // can immediately follow up with a raw read on any returned handle. `addr` names the base field.
    template <typename T>
    int32_t bridgeList(const CubeApi* api, const char* label, const T* out, int32_t maxCount, int32_t count, uint32_t T::* addr)
    {
        constexpr int32_t kMaxLogged = 16; // cap so a full inventory/entity sweep cannot flood one line
        char buf[kMaxLogged * 11 + 16] = {};
        int written = 0;
        const int32_t shown = (count < kMaxLogged) ? count : kMaxLogged;
        for (int32_t i = 0; i < shown && written >= 0 && written < static_cast<int>(sizeof(buf)); ++i)
        {
            const int n = std::snprintf(buf + written, sizeof(buf) - written, "%s0x%08X", i ? " " : "", out[i].*addr);
            if (n < 0)
                break;
            written += n;
        }
        if (count > shown && written >= 0 && written < static_cast<int>(sizeof(buf)))
            std::snprintf(buf + written, sizeof(buf) - written, " +%d more", count - shown);
        LOGC(Trace, kApiCategory, "'%s' %s(max %d) -> %d [%s]", modName(api), label, maxCount, count, buf);
        return count;
    }

    // Setter reducers, one per argument-shape so the log format matches.
    inline int32_t bridgeSetAddrField(const CubeApi* api, const char* label,
                                      bool (*set)(uint32_t, int32_t, double),
                                      uint32_t address, int32_t field, double value)
    {
        writeguard::Scope scope(api);
        const bool ok = set(address, field, value);
        LOGC(Debug, kApiCategory, "'%s' %s(0x%08X, %d, %.3f) -> %s",
             modName(api), label, address, field, value, ok ? "ok" : "fail");
        return okInt(ok);
    }

    inline int32_t bridgeSetIndexed(const CubeApi* api, const char* label, bool (*set)(int32_t, double), int32_t field, double value)
    {
        writeguard::Scope scope(api);
        const bool ok = set(field, value);
        LOGC(Debug, kApiCategory, "'%s' %s(%d, %.3f) -> %s",
             modName(api), label, field, value, ok ? "ok" : "fail");
        return okInt(ok);
    }

    inline int32_t bridgeSetPair(const CubeApi* api, const char* label, bool (*set)(int32_t, int32_t), int32_t first, int32_t second)
    {
        writeguard::Scope scope(api);
        const bool ok = set(first, second);
        LOGC(Debug, kApiCategory, "'%s' %s(%d, %d) -> %s",
             modName(api), label, first, second, ok ? "ok" : "fail");
        return okInt(ok);
    }

    inline int32_t bridgeSetVec3(const CubeApi* api, const char* label, bool (*set)(float, float, float), float x, float y, float z)
    {
        writeguard::Scope scope(api);
        const bool ok = set(x, y, z);
        LOGC(Debug, kApiCategory, "'%s' %s(%.1f, %.1f, %.1f) -> %s",
             modName(api), label, x, y, z, ok ? "ok" : "fail");
        return okInt(ok);
    }

    inline int32_t bridgeSetAddrVec3(const CubeApi* api, const char* label, bool (*set)(uint32_t, float, float, float), uint32_t address, float x, float y, float z)
    {
        writeguard::Scope scope(api);
        const bool ok = set(address, x, y, z);
        LOGC(Debug, kApiCategory, "'%s' %s(0x%08X, %.1f, %.1f, %.1f) -> %s",
             modName(api), label, address, x, y, z, ok ? "ok" : "fail");
        return okInt(ok);
    }

    inline int32_t bridgeSetName(const CubeApi* api, const char* label, bool (*set)(const char*), const char* name)
    {
        writeguard::Scope scope(api);
        const bool ok = set(name);
        LOGC(Debug, kApiCategory, "'%s' %s(%s) -> %s",
             modName(api), label, name ? name : "(null)", ok ? "ok" : "fail");
        return okInt(ok);
    }

    inline int32_t bridgeSetAddrName(const CubeApi* api, const char* label, bool (*set)(uint32_t, const char*), uint32_t address, const char* name)
    {
        writeguard::Scope scope(api);
        const bool ok = set(address, name);
        LOGC(Debug, kApiCategory, "'%s' %s(0x%08X, %s) -> %s",
             modName(api), label, address, name ? name : "(null)", ok ? "ok" : "fail");
        return okInt(ok);
    }

    void fillSystem(CubeApi& api);
    void fillPlayer(CubeApi& api);
    void fillCombat(CubeApi& api);
    void fillItems(CubeApi& api);
    void fillStatus(CubeApi& api);
    void fillWorld(CubeApi& api);
    void fillEntities(CubeApi& api);
    void fillPet(CubeApi& api);
    void fillView(CubeApi& api);
    void fillSession(CubeApi& api);
    void fillCatalog(CubeApi& api);
    void fillInput(CubeApi& api);
    void fillSelection(CubeApi& api);
    void fillPickup(CubeApi& api);
    void fillHooks(CubeApi& api);
    void fillConfig(CubeApi& api);
    void fillStorage(CubeApi& api);
    void fillServices(CubeApi& api);
}