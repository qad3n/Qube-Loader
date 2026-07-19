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
    // --- appended in ABI 21 (persistence); older mods never read past here, so growth stays additive ---
    CubeConfigApi config; // per-mod user-editable settings (<dllDir>/config/<stem>.ini)
    CubeStorageApi storage; // per-mod binary save data (<dllDir>/data/<stem>/)
    // --- appended in ABI 22 (ecosystem) ---
    CubeServicesApi services; // shared-service registry + directed inter-mod messaging (by manifest id)
    // --- appended in ABI 23 (localization) ---
    CubeLocaleApi locale; // per-mod string translation (<dllDir>/lang/<stem>/<locale>.ini)
} CubeApi;

// One declared dependency on another mod (CubeModInfo::deps, a null-terminated array). The loader
// resolves these by id after all mods load and before CUBE_EVENT_READY.
typedef struct CubeModDep
{
    const char* id;         // required mod's id (array terminator = entry whose id is NULL)
    const char* minVersion; // minimum acceptable version string, NULL/"" = any
    int32_t hard;           // 1 = hard (refuse to load this mod if unmet), 0 = soft (load anyway)
} CubeModDep;

typedef struct CubeModInfo
{
    uint32_t structSize;
    const char* name;
    const char* version;
    const char* author;
    // Dispatch priority: higher runs LAST in every event/hook reduce (final say on last-writer-wins
    // returns); ties keep load order. Set via Mod::setPriority in the mod body; 0 = default.
    int32_t priority;
    // --- appended in ABI 20; each read only under a structSize gate so older mods still load ---
    const char* id;         // stable machine id (unique); loader falls back to the DLL stem if NULL
    uint32_t requiredAbi;   // ABI the mod was built against (CUBE_MOD sets it automatically); 0 = unspecified
    uint32_t capabilities;  // CubeModCapability bitset; 0 = unrestricted
    const CubeModDep* deps; // null-terminated dependency array; NULL = none
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
