#pragma once
// Enums + the forward CubeApi declaration.

#include "cube_sdk/core.h"

#define CUBE_MOD_API CUBE_EXTERN_C __declspec(dllexport)

// Which process this mod instance is running in; check it to branch behavior.
// Client-only data (camera/display/overlay) is unavailable on the server.
typedef enum CubeEnvironment
{
    CUBE_ENV_CLIENT = 0,
    CUBE_ENV_SERVER
} CubeEnvironment;

// Declared powers a mod uses (CubeModInfo::capabilities bitset). 0 = undeclared = unrestricted (the
// loader gates a mod only once it declares a non-zero set); see api::fill capability stubbing.
typedef enum CubeModCapability
{
    CUBE_CAP_NONE = 0,
    CUBE_CAP_RAW_MEM = 1 << 0,   // mem.read/readable/write over arbitrary addresses
    CUBE_CAP_RAW_HOOKS = 1 << 1, // hooks.onRaw / rawDetour on user addresses
    CUBE_CAP_WRITES = 1 << 2,    // guarded game-state writes (setStat/setField)
    CUBE_CAP_OVERLAY = 1 << 3    // render/input surfaces (camera/display/ui/input)
} CubeModCapability;

typedef enum CubeLogLevel
{
    CUBE_LOG_TRACE = 0,
    CUBE_LOG_DEBUG,
    CUBE_LOG_INFO,
    CUBE_LOG_WARN,
    CUBE_LOG_ERROR
} CubeLogLevel;

typedef enum CubeClass
{
    CUBE_CLASS_UNKNOWN = 0,
    CUBE_CLASS_WARRIOR = 1,
    CUBE_CLASS_RANGER = 2,
    CUBE_CLASS_MAGE = 3,
    CUBE_CLASS_ROGUE = 4
} CubeClass;

// Each edge carries only data the loader actually reads at that edge (poll-derived unless noted);
// no value is invented. The payload slots are args.subject (uint32 address), args.param /
// args.param2 (int32), and args.amount (float). "unused" slots stay 0. See CubeEventArgs.
typedef enum CubeEvent
{
    CUBE_EVENT_STARTUP = 0,
    CUBE_EVENT_SHUTDOWN,
    CUBE_EVENT_FRAME, // once per rendered frame; args.device / args.hwnd valid (client)
    CUBE_EVENT_DEVICE_RESET, // args.preReset: 1 before reset (release), 0 after (recreate)
    CUBE_EVENT_WNDPROC, // a window message; set args.swallow=1 to consume it (client)
    CUBE_EVENT_PLAYER_ATTACK, // local player performed one PRIMARY attack (a left-click swing or shot).
                              // Fires ONCE PER swing/shot - including every step of a melee combo and each
                              // repeated ranged shot (detected on the game thread from the per-swing timer
                              // reset, so sub-frame shots are not missed). Hotbar abilities are NOT this
                              // event - see CUBE_EVENT_ABILITY_USED. subject = player, param = raw action id
                              // at the edge, param2 = the selected target address (0 if none; not a
                              // confirmed victim - the swing has not landed yet).
    CUBE_EVENT_PLAYER_JUMP, // local player left the ground (polled edge). subject = player,
                            // amount = vertical velocity (velZ) read at the edge
    CUBE_EVENT_AREA_CHANGE, // local player entered a different zone (polled edge). param = new zoneX,
                            // param2 = new zoneY
    CUBE_EVENT_PLAYER_DAMAGED, // local player lost health this frame. subject = player, amount = damage
                               // (HP lost, float), param = the same as int, param2 = remaining health (int).
                               // Post-mitigation HP delta the loader sees by diffing health, NOT the game's
                               // raw damage number; cause is not attributed. For the attacker + the game's
                               // damage value (and to cancel/rescale) use eventHook().onImpact.
    CUBE_EVENT_ENTITY_DAMAGED, // ANY nearby creature lost health this frame (NOT necessarily player-caused).
                           // subject = victim, amount = damage (HP lost, float), param = victim category byte,
                           // param2 = victim remaining health (int). Fires once per damaged creature; the
                           // attacker is NOT attributed (a monster hitting a monster fires this too). For
                           // the player's own confirmed, attributed hits use eventHook().onImpact instead.
    CUBE_EVENT_PLAYER_CRIT, // local player landed a critical hit (detour-backed; no extra payload)
    CUBE_EVENT_MENU_OPEN, // a tracked UI menu opened (polled edge). param = CubeUiMenuMask of open panels
    CUBE_EVENT_MENU_CLOSE, // all tracked UI menus closed (polled edge). param = CubeUiMenuMask (0 when all closed)
    CUBE_EVENT_PLAYER_LEVELUP, // local player's level increased. subject = player, param = new level,
                               // param2 = previous level
    CUBE_EVENT_PLAYER_DEATH, // local player's health reached zero (polled edge). subject = player
    CUBE_EVENT_PLAYER_RESPAWN, // local player returned to life after dying (polled edge). subject = player
    CUBE_EVENT_PLAYER_LAND, // local player touched down (airborne -> grounded; complements JUMP).
                            // subject = player, amount = vertical velocity (velZ) read at the edge
    CUBE_EVENT_MOVEMENT_CHANGED, // locomotion mode changed. subject = player, param = new CubeMovement,
                                 // param2 = previous CubeMovement
    CUBE_EVENT_TARGET_CHANGED, // selection/interact target changed. subject = new creature (0 if cleared),
                               // param = previous target address
    CUBE_EVENT_ENTITY_SPAWN, // a creature was created / entered range. subject = creature,
                             // param = its category byte, param2 = its type/species id, amount = its health
    CUBE_EVENT_ENTITY_DEATH, // a tracked creature's health reached zero (killed). subject = creature,
                             // param = its category byte, param2 = its type/species id
    CUBE_EVENT_COINS_CHANGED, // coin total changed. param = new coin total, param2 = delta (new - old)
    CUBE_EVENT_DAY_NIGHT, // day/night flipped. param = 1 day / 0 night, param2 = in-game hour [0,23]
    CUBE_EVENT_BUFF_GAINED, // a status effect was applied. param = effect type id, param2 = remaining
                            // duration ms, amount = magnitude (looked up by type; first match if duplicated)
    CUBE_EVENT_BUFF_LOST, // a status effect ended; param = effect type id (the effect is gone, no magnitude)
    CUBE_EVENT_EQUIPMENT_CHANGED, // an equipment slot changed. param = slot index, param2 = item type now
                                  // in the slot (0 if the slot was emptied)
    CUBE_EVENT_SKILL_RANK_CHANGED, // a skill rank changed. param = skill index, param2 = new rank
    CUBE_EVENT_AIM_TARGET_CHANGED, // crosshair hover target changed; args.subject = creature (0 if none)
    CUBE_EVENT_ENTITY_DESPAWN, // a tracked creature was destroyed / left range. subject = its last address,
                               // param = its last-known category byte, param2 = its last-known type id
    CUBE_EVENT_PET_SUMMONED, // the local player's pet was created / changed; args.subject = pet creature
    CUBE_EVENT_PET_DIED, // the local player's pet's health reached zero; args.subject = pet creature
    CUBE_EVENT_PET_DISMISSED, // the local player's pet was destroyed / dismissed; args.subject = last address
    CUBE_EVENT_PLAYER_STUNNED, // the LOCAL player's stun-lock became active from taking damage (cannot act);
                               // args.subject = player, args.param = stun-lock timer. The shared lock the game
                               // also uses for a dodge-roll is filtered out (that raises PLAYER_ROLL instead),
                               // so this fires on a genuine hit / hard fall. For creatures use ENTITY_STUNNED.
    CUBE_EVENT_PLAYER_KNOCKED_DOWN, // the LOCAL player entered the downed state (on-ground, stars). subject = player
    CUBE_EVENT_PLAYER_RECOVERED, // the LOCAL player's stun-lock ended (can act again). subject = player.
                               // Pairs with PLAYER_STUNNED, so it likewise follows a hard fall/collision, not
                               // just an enemy stun.
    CUBE_EVENT_ENTITY_STUNNED, // a nearby creature became stunned. subject = creature, param = category byte, param2 = type id
    CUBE_EVENT_ENTITY_KNOCKED_DOWN, // a nearby creature was knocked down. subject = creature, param = category, param2 = type id
    CUBE_EVENT_PET_STUNNED, // the local player's pet became stunned; args.subject = pet creature
    CUBE_EVENT_PET_KNOCKED_DOWN, // the local player's pet was knocked down; args.subject = pet creature
    CUBE_EVENT_ENTITY_SELECTED, // the player pressed the R / use key (GameController::updateSelectedEntity);
                         // args.subject = selected target creature (0 for a world object),
                         // args.param = CubeSelectionKind, args.param2 = raw target discriminator byte
                         // (CubeSelection.typeByte)
    CUBE_EVENT_ENTITY_RECOVERED, // a nearby creature's stun-lock ended (complements ENTITY_STUNNED).
                                 // subject = creature, param = category byte, param2 = type id
    CUBE_EVENT_PET_RECOVERED, // the local player's pet's stun-lock ended; args.subject = pet creature
    CUBE_EVENT_ABILITY_USED, // the local player used a hotbar ability (keys 1-5). subject = player,
                             // param = ability id (matches CUBE_CATALOG_ABILITY), param2 = cooldown ms set.
    CUBE_EVENT_ITEM_PICKUP, // the local player picked up an item (E / hold-to-pickup completes).
                            // subject = item type, param = subtype, param2 = stack count. Use
                            // pickup.getLast() (or the onItemPickup hpp lambda) for the full CubeItem.
    CUBE_EVENT_PLAYER_ROLL, // the LOCAL player performed a dodge-roll (stamina dash + brief action
                            // lock; the action reads CUBE_ACTION_ROLLING for its duration). subject =
                            // player, amount = vertical pop velocity. Distinct from PLAYER_JUMP: the
                            // roll's upward pop no longer misfires as a jump / hit-stun.
    CUBE_EVENT_READY, // all mods have loaded and passed dependency resolution; emitted once on the load
                      // thread after STARTUP and before the render arm. Safe point to resolve another
                      // mod's registered service (see CubeServicesApi). No payload.
    CUBE_EVENT_WORLD_ENTER, // the local player became resident in a world (title/menu -> in-world edge).
                            // subject = player. Distinct from AREA_CHANGE (zone-to-zone within a world).
    CUBE_EVENT_WORLD_EXIT, // the local player left the world (in-world -> title/menu edge). subject = 0.
    CUBE_EVENT_COUNT
} CubeEvent;

// Mutually-exclusive locomotion, resolved by the loader (see hero.getMovement()).
typedef enum CubeMovement
{
    CUBE_MOVE_UNKNOWN = 0,
    CUBE_MOVE_GROUNDED,
    CUBE_MOVE_AIRBORNE,
    CUBE_MOVE_CLIMBING,
    CUBE_MOVE_SWIMMING,
    CUBE_MOVE_GLIDING,
    CUBE_MOVE_SNEAKING,
    CUBE_MOVE_SITTING
} CubeMovement;

// Current action / animation intent, resolved by the loader (see hero.getAction()).
typedef enum CubeAction
{
    CUBE_ACTION_UNKNOWN = 0,
    CUBE_ACTION_IDLE,
    CUBE_ACTION_ATTACKING,
    CUBE_ACTION_CASTING,
    CUBE_ACTION_BLOCKING,
    CUBE_ACTION_ROLLING,
    CUBE_ACTION_DEAD
} CubeAction;

// Editable local-player scalar stats (see player.setStat); value is a double so one entry point covers float and int fields.
typedef enum CubePlayerStat
{
    CUBE_STAT_HEALTH = 0,
    CUBE_STAT_MANA,
    CUBE_STAT_STAMINA,
    CUBE_STAT_COINS,
    CUBE_STAT_LEVEL,
    CUBE_STAT_XP,
    CUBE_STAT_CLASS,
    CUBE_STAT_SPEC,
    CUBE_STAT_TYPE,
    CUBE_STAT_FACING,
    CUBE_STAT_POWER,
    CUBE_STAT_ARMOR,
    CUBE_STAT_SPIRIT,
    CUBE_STAT_COMBO,
    CUBE_STAT_ATTACK_COOLDOWN,
    CUBE_STAT_HITSTUN,
    CUBE_STAT_VEL_X,
    CUBE_STAT_VEL_Y,
    CUBE_STAT_VEL_Z,
    CUBE_STAT_ACTION_ID, // raw action/animation byte
    CUBE_STAT_CATEGORY, // entity category byte (monster/player/npc/hostile)
    CUBE_STAT_RANK, // entity star/power rank byte
    CUBE_STAT_ATTACK_SPEED, // attack-timescale float (higher = faster)
    CUBE_STAT_CRIT, // crit attribute float (0.15 crit chance per point)
    CUBE_STAT_SNEAKING, // 1 = enable stealth (write the stealth stat), 0 = disable it
    CUBE_STAT_BASE_DAMAGE, // base attack-damage input float (scales every attack/ability)
    CUBE_STAT_LANTERN // 1 = turn the held lantern/light on, 0 = off
} CubePlayerStat;

// Editable fields of a single item (see items.setField), mapped to the item POD.
typedef enum CubeItemField
{
    CUBE_ITEM_FIELD_TYPE = 0,
    CUBE_ITEM_FIELD_SUBTYPE,
    CUBE_ITEM_FIELD_MATERIAL,
    CUBE_ITEM_FIELD_MODIFIER,
    CUBE_ITEM_FIELD_LEVEL,
    CUBE_ITEM_FIELD_UPGRADE_COUNT,
    CUBE_ITEM_FIELD_SEED,
    CUBE_ITEM_FIELD_STACK // inventory stack count (in the cell, before the item body)
} CubeItemField;

// Editable session/network fields (see session.setField).
typedef enum CubeSessionField
{
    CUBE_SESSION_NETWORK_MODE = 0, // 0 singleplayer, 1 multiplayer
    CUBE_SESSION_CONNECTED // networking-active byte
} CubeSessionField;

// Editable HUD-open flags (see ui.setField).
typedef enum CubeUiField
{
    CUBE_UI_INVENTORY = 0,
    CUBE_UI_CHARACTER,
    CUBE_UI_MAP,
    CUBE_UI_OBJECTIVE
} CubeUiField;

// Bit flags for MENU_OPEN/MENU_CLOSE args.param: which tracked HUD panels are open at the edge.
typedef enum CubeUiMenuMask
{
    CUBE_MENU_INVENTORY = 1 << 0,
    CUBE_MENU_CHARACTER = 1 << 1,
    CUBE_MENU_MAP = 1 << 2,
    CUBE_MENU_OBJECTIVE = 1 << 3
} CubeUiMenuMask;

// Editable fields of the player's current ZoneTile (see world.setTile).
typedef enum CubeTileField
{
    CUBE_TILE_TERRAIN = 0,
    CUBE_TILE_TEMPERATURE,
    CUBE_TILE_HUMIDITY,
    CUBE_TILE_ELEVATION
} CubeTileField;

// Editable fields of a placed structure / POI (see world.setStructure).
typedef enum CubeStructureField
{
    CUBE_STRUCT_TYPE = 0,
    CUBE_STRUCT_POS_X,
    CUBE_STRUCT_POS_Y,
    CUBE_STRUCT_POS_Z
} CubeStructureField;

// Editable fields of an active status effect / buff (see status.setField).
typedef enum CubeBuffField
{
    CUBE_BUFF_TYPE = 0,
    CUBE_BUFF_MAGNITUDE,
    CUBE_BUFF_DURATION
} CubeBuffField;

// Editable camera scalars (see camera.set). FOV is a hardcoded constant, not stored.
typedef enum CubeCameraField
{
    CUBE_CAM_DISTANCE = 0,
    CUBE_CAM_PITCH,
    CUBE_CAM_YAW
} CubeCameraField;

// Editable display / graphics settings (see display.set), the options.cfg globals.
typedef enum CubeDisplayField
{
    CUBE_DISPLAY_FULLSCREEN = 0,
    CUBE_DISPLAY_RESOLUTION_X,
    CUBE_DISPLAY_RESOLUTION_Y,
    CUBE_DISPLAY_RENDER_DISTANCE,
    CUBE_DISPLAY_SOUND_VOLUME,
    CUBE_DISPLAY_MUSIC_VOLUME,
    CUBE_DISPLAY_MIN_TIMESTEP
} CubeDisplayField;

struct CubeApi;

