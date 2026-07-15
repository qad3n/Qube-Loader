#pragma once
// The top-level CubeApi struct, CubeModInfo, the mod entry/shutdown typedefs, and cubeLogf.

#include "cube_sdk/apis.h"

typedef struct CubeApi
{
    uint32_t abiVersion;
    uint32_t structSize;
    CubeEnvironment environment; // client or server
    CubeLogApi log;
    CubeMemApi mem;
    CubeEventApi events;
    CubePlayerApi player;
    CubeCombatApi combat;
    CubeItemsApi items;
    CubeSkillsApi skills;
    CubeStatusApi status;
    CubeWorldApi world;
    CubePetApi pet;
    CubeEntitiesApi entities;
    CubeCameraApi camera; // client only
    CubeDisplayApi display; // client only
    CubeAudioApi audio; // client only
    CubeSessionApi session;
    CubeUiApi ui; // client only
    CubeCatalogApi catalog;
    CubeInputApi input; // client only: freeze game input while an overlay is open
    CubeSelectionApi selection; // client only: last-selected record (R / use key), detour-backed
    CubePickupApi pickup; // client only: last-picked-up item (E key), detour-backed
    CubeHooksApi hooks; // game-function interception (cancel/modify/override), separate from events
} CubeApi;

typedef struct CubeModInfo
{
    uint32_t structSize;
    const char* name;
    const char* version;
    const char* author;
    // Dispatch priority: higher runs LAST in every event/hook reduce (final say on last-writer-wins
    // returns); ties keep load order. Set via Mod::setPriority in the mod body; 0 = default.
    int32_t priority;
} CubeModInfo;

// Required export: called once when the mod is loaded. Return a static CubeModInfo.
typedef CubeModInfo* (CUBE_CALL* CubeModInitFn)(const CubeApi* api);
// Optional export: called once just before the mod is unloaded.
typedef void (CUBE_CALL* CubeModShutdownFn)(void);

static inline void cubeLogf(const CubeApi* api, CubeLogLevel level, const char* fmt, ...)
{
    char buffer[CUBE_LOG_BUFFER];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    api->log.write(api, level, buffer);
}
