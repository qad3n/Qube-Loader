#pragma once
// ABI preamble: version, size limits, and calling-convention/export macros.

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

#define CUBE_ABI_VERSION 22
// Oldest mod ABI this loader still accepts. Held FIXED as CUBE_ABI_VERSION grows (growth is
// additive-only), so a mod built against any ABI in [CUBE_MIN_ABI_VERSION, CUBE_ABI_VERSION] keeps
// loading. Only raise it if a non-additive break makes older mods genuinely incompatible.
#define CUBE_MIN_ABI_VERSION 20
#define CUBE_LOG_BUFFER 1024
#define CUBE_CONFIG_STRING_MAX 256
#define CUBE_HOOK_ARG_MAX 4
#define CUBE_PLAYER_NAME_MAX 64
#define CUBE_CLASS_NAME_MAX 16
#define CUBE_AREA_NAME_MAX 64
#define CUBE_ENTITIES_MAX 512
#define CUBE_ITEM_NAME_MAX 24
#define CUBE_EQUIP_SLOTS 12
#define CUBE_INVENTORY_MAX 128
#define CUBE_SKILL_COUNT 11
#define CUBE_STRUCTURES_MAX 64
#define CUBE_BUFFS_MAX 32
#define CUBE_ABILITIES_MAX 32
#define CUBE_CATALOG_MAX 192

#define CUBE_CALL __cdecl

#ifdef __cplusplus
#define CUBE_EXTERN_C extern "C"
#else
#define CUBE_EXTERN_C
#endif

