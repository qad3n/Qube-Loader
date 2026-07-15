#pragma once
// POD data structs returned across the ABI (player/world/entity/item/... snapshots).

#include "cube_sdk/enums.h"

typedef struct CubePlayer
{
    uint32_t structSize;
    uint32_t address; // live Creature base pointer (0 if unavailable; for raw use)
    float health; // current HP (float in-game)
    int32_t alive; // 1 while HP > 0
    int32_t level;
    uint32_t xp;
    int32_t type; // creature type / model id (race for players)
    int32_t classForm; // CubeClass (1..4)
    char className[CUBE_CLASS_NAME_MAX]; // ready-to-print class name, computed by the loader
    int32_t spec; // specialization byte
    char name[CUBE_PLAYER_NAME_MAX];
    int32_t hasName; // 1 if name was resolved
    float x;
    float y;
    float z;
    int32_t hasPosition; // 1 if position was resolved
    int32_t actionId; // raw action / animation id (advanced; prefer `action`)
    int32_t movement; // CubeMovement, resolved by the loader (grounded/air/climbing/swimming...)
    int32_t action; // CubeAction, resolved by the loader (idle/attacking/casting...)
    int32_t onGround; // 1 grounded, 0 airborne (legacy; == movement==GROUNDED)
    int32_t attacking; // 1 if attacking this frame (legacy; == action==ATTACKING)
    int32_t hasState; // 1 if the action-state fields were resolved
    float mana; // class resource, normalized 0..1 (use for a bar)
    float manaPercent; // ready-to-display percentage 0..100, computed by the loader
    float stamina; // normalized 0..1 (use for a bar)
    float staminaPercent; // ready-to-display percentage 0..100, computed by the loader
    int32_t coins; // money / currency
    float facing; // look/camera yaw, radians (stable while airborne)
    float velX;
    float velY;
    float velZ; // vertical axis
    float speed; // horizontal speed magnitude, computed by the loader
    uint32_t target; // selected/interact entity address (0 if none)
    int32_t sneaking; // 1 if stealthed (the stealth stat > 0; Cube World has no crouch bit)
    int32_t knockedDown; // 1 if in the knocked-down action state (heavy-hit stagger)
    int32_t actionElapsedMs; // ms elapsed in the current action (resets to 0 on any action change)
    float stealth; // stealth/sneak stat 0..1 (reduces enemy detection; also feeds crit chance)
    int32_t lantern; // 1 if the held lantern / light is on (render flag; toggle via CUBE_STAT_LANTERN)
} CubePlayer;

// Local-player combat snapshot. Stored stats are direct reads; the damage/hit
// counters are maintained by the loader's per-frame poll (see CUBE_EVENT_PLAYER_*).
typedef struct CubeCombat
{
    uint32_t structSize;
    uint32_t address; // live Creature base pointer (0 if unavailable)
    float baseDamage; // base attack-damage input (scales every attack/ability; editable)
    float power; // spell/attack power base (Creature field)
    float armor; // armor/defense base
    float spirit; // spirit base
    int32_t combo; // combo / rune counter
    float attackCooldown; // attack windup remaining (ready to strike when <= 0)
    float attackSpeed; // attack-timescale factor (higher = faster attacks)
    float critStat; // crit attribute (0.15 crit chance per point)
    float critChancePercent; // approx crit chance %, = critStat*15 (excludes gear bonus)
    int32_t hitStun; // hit-stun / knockback timer (0..600, 0 = free to act)
    float lastDamageTaken; // HP lost in the most recent damage tick (0 if none yet)
    float lastDamageDealt; // approximate damage dealt to the target on the last hit
    uint32_t hits; // running count of hits the player dealt this session
    uint32_t crits; // running count of the local player's critical hits this session (detour-backed via CRIT_ROLL)
    uint32_t damageTakenEvents; // running count of times the player took damage
    int32_t hasCombat; // 1 if the combat fields were resolved
} CubeCombat;

typedef struct CubeWorld
{
    uint32_t structSize;
    uint32_t address; // live GameController base pointer (0 if unavailable)
    int32_t zoneX; // current zone coordinate
    int32_t zoneY;
    int32_t regionX; // current region coordinate (zone >> 6)
    int32_t regionY;
    int32_t hasArea; // 1 if the zone/region fields were resolved
    int32_t terrainType; // ZoneTile terrain-type id (only if hasClimate)
    float temperature; // ZoneTile temperature (raw game value)
    float humidity; // ZoneTile humidity (raw game value)
    float elevation; // ZoneTile region elevation/height (raw game value)
    int32_t hasClimate; // 1 if the player's tile is resident (temp/humidity valid)
    int32_t timeMs; // time-of-day, milliseconds in [0, 86400000)
    int32_t hour; // hour of day [0,23], computed by the loader (valid if hasTime)
    int32_t minute; // minute [0,59], computed by the loader (valid if hasTime)
    int32_t hasTime; // 1 if the day-clock was resolved
    int32_t isDay; // 1 if the in-game hour is daytime (valid if hasTime)
    float spawnX; // player's bound spawn point (valid if hasSpawn)
    float spawnY;
    float spawnZ;
    int32_t hasSpawn;
    uint32_t seed; // world generation seed (valid if hasSeed; R5 gated, currently 0)
    int32_t hasSeed;
    // NOTE: no weather system and no biome name exist in this build (biome is computed from temp/humidity).
} CubeWorld;

// A placed structure / prop / POI from the player's current zone. `type` is the kind id,
// `name` its resolved name (CUBE_CATALOG_STRUCTURE_TYPE). NOTE: this alpha has no stored
// dungeon-level / region-difficulty / village-town object; enemy strength is per-creature.
typedef struct CubeStructure
{
    uint32_t structSize;
    uint32_t address; // record base pointer
    int32_t type; // kind/type id
    float x;
    float y;
    float z;
    char name[CUBE_ITEM_NAME_MAX]; // resolved structure name, computed by the loader
} CubeStructure;

typedef struct CubePet
{
    uint32_t structSize;
    uint32_t address; // live pet Creature base pointer (0 if unavailable)
    int32_t type; // creature type / model id
    int32_t level;
    uint32_t xp;
    float health;
    char name[CUBE_PLAYER_NAME_MAX];
    int32_t hasName;
    float x;
    float y;
    float z;
    int32_t hasPosition;
    int32_t entityState; // CubeEntityState (alive/dead)
    float facing; // body yaw, radians
    float velX;
    float velY;
    float velZ;
    // Pet mood/loyalty/hunger (R4) is not located in this build - no field exposed.
} CubePet;

// An active status effect / buff / debuff on the local player. `type` is the raw
// effect id; the enum meaning is only partially mapped, so the raw id is exposed.
typedef struct CubeBuff
{
    uint32_t structSize;
    uint32_t address; // live effect-node base pointer (0 if unavailable; for editing)
    int32_t type; // effect kind id
    float magnitude; // effect amount
    int32_t remainingMs; // duration remaining in milliseconds
} CubeBuff;

// What the player last selected with the R / use key, decoded by the loader. WORLD
// is a non-creature object (door / lever / campfire) - the target address is then 0.
typedef enum CubeSelectionKind
{
    CUBE_SELECTION_NONE = 0,
    CUBE_SELECTION_CREATURE, // selected a creature (e.g. talked to an NPC)
    CUBE_SELECTION_CONTAINER, // opened a loot/trade container (chest / merchant)
    CUBE_SELECTION_DIALOG, // opened a dialog / selection widget
    CUBE_SELECTION_WIDGET, // opened another select widget
    CUBE_SELECTION_WORLD // selected a non-creature world object (door / lever / etc.)
} CubeSelectionKind;

// The most recent selection (R / use key), captured by a detour so it is exact. Use it to
// react to "player selected X" - e.g. onEntitySelected + check kind / address.
typedef struct CubeSelection
{
    uint32_t structSize;
    uint32_t address; // the selected creature/object (0 for a world object)
    int32_t typeByte; // raw target discriminator (+0x140): 0x80-0x82 container, 0x83 dialog,
                      // 0x89 widget, or a creature's class 1..4
    int32_t kind; // CubeSelectionKind, decoded by the loader
    uint32_t count; // total selections observed this session (a monotonically rising counter)
    int32_t valid; // 1 once at least one selection has been captured
} CubeSelection;

// Consolidated stun / knocked-down snapshot for one creature: the downed action and
// the stun-lock timer that blocks acting, resolved into one value plus the knockback.
typedef struct CubeStun
{
    uint32_t structSize;
    uint32_t address; // creature this describes (0 if unavailable)
    int32_t knockedDown; // 1 if in the downed action state (on the ground, the "stars" pose)
    int32_t stunned; // 1 if the stun-lock timer is active (cannot act/move)
    int32_t hitStun; // stun-lock timer, 0..600 (0 = free to act)
    float hitStunPercent; // hitStun as a 0..100 percentage of the max, computed by the loader
    float knockbackX; // knockback velocity written on impact
    float knockbackY;
    int32_t valid; // 1 if the fields were resolved
} CubeStun;

// A single item: an equipment slot's item or an inventory cell's item. `type`==0
// means an empty slot. `typeName` is the coarse category; `name` is the resolved item name.
typedef struct CubeItem
{
    uint32_t structSize;
    uint32_t address; // live item base pointer (0 if empty)
    int32_t present; // 1 if this slot/cell holds an item
    int32_t slot; // equipment slot index (0..11), or -1 for inventory items
    int32_t stack; // stack count (inventory), 1 for an equipped item
    int32_t type; // item type / category (1=consumable, 3=weapon, 20=food, 21=accessory, ...)
    int32_t subtype;
    int32_t material;
    int32_t modifier; // rune / affix
    int32_t level; // item level; also the heal/nourishment amount for consumables/food
    uint32_t seed; // stat-roll variance seed
    int32_t upgradeCount; // spirit / upgrade count
    char name[CUBE_ITEM_NAME_MAX]; // resolved item name (type,subtype -> registry name), computed by the loader
    char typeName[CUBE_ITEM_NAME_MAX]; // coarse category name, computed by the loader
} CubeItem;

// How an entity relates to the local player (resolved by the loader).
typedef enum CubeRelation
{
    CUBE_REL_UNKNOWN = 0,
    CUBE_REL_SELF,
    CUBE_REL_OWN_PET,
    CUBE_REL_PLAYER, // another player
    CUBE_REL_NPC,
    CUBE_REL_NEUTRAL, // non-aggressive monster
    CUBE_REL_HOSTILE
} CubeRelation;

typedef enum CubeEntityState
{
    CUBE_ENTSTATE_UNKNOWN = 0,
    CUBE_ENTSTATE_ALIVE,
    CUBE_ENTSTATE_DYING,
    CUBE_ENTSTATE_DEAD
} CubeEntityState;

typedef struct CubeEntity
{
    uint32_t structSize;
    uint32_t address; // live Creature base pointer
    int32_t category; // raw Creature kind byte +0x60: 0 player, 1 monster, 5 pet, 6 npc (3 unlabeled)
    int32_t type; // creature type / model id
    int32_t level;
    float health;
    char name[CUBE_PLAYER_NAME_MAX];
    int32_t hasName;
    float x;
    float y;
    float z;
    float distance; // world blocks from the local player
    int32_t hostile; // 1 if hostile (legacy; == relation==HOSTILE)
    int32_t relation; // CubeRelation, resolved by the loader
    int32_t entityState; // CubeEntityState (alive/dying/dead)
    int32_t boss; // 1 if a boss-type species
    int32_t elite; // 1 if a standout enemy (derived: boss or star rank >= 3, non-player)
    int32_t rank; // star / power rank (0 = common; the monster power indicator, non-player)
    int32_t combatClass; // combat archetype byte +0x140 (1 Warrior/2 Ranger/3 Mage/4 Rogue)
    int32_t effectivePower; // computed challenge = level/2 + star + 1 (compare vs your level)
    int32_t hitStun; // stagger/flinch timer (0..600; >0 means staggered)
    int32_t knockedDown; // 1 if in the knocked-down action state
    uint32_t ownerAddress; // owner Creature address if this is a pet/summon (0 otherwise)
    float facing; // body yaw, radians
    float velX;
    float velY;
    float velZ;
    CubeItem weapon; // equipped main-hand item (weapon.present==0 if none)
} CubeEntity;

typedef struct CubeCamera
{
    uint32_t structSize;
    uint32_t address; // live GameController base pointer (0 if unavailable; for raw use)
    float distance; // 3rd-person zoom
    float pitch; // game units (0..180)
    float yaw; // game units
    float fov; // radians (fixed pi/4)
    int32_t valid;
    float view[16]; // 4x4 view matrix (valid if hasMatrices)
    float proj[16]; // 4x4 projection matrix (valid if hasMatrices)
    int32_t hasMatrices;
    int32_t firstPerson; // 1 first-person, 0 third-person (valid if hasMode; R9 gated)
    int32_t hasMode;
} CubeCamera;

typedef struct CubeDisplay
{
    uint32_t structSize;
    uint32_t address; // live GameController base pointer (0 if unavailable; for raw use)
    int32_t width; // backbuffer width
    int32_t height; // backbuffer height
    int32_t fullscreen; // 1 fullscreen, 0 windowed
    int32_t valid;
    int32_t renderDistance; // view/render distance setting (valid if hasSettings)
    int32_t resolutionX; // configured resolution (valid if hasSettings)
    int32_t resolutionY;
    int32_t soundVolume; // 0..100 (valid if hasSettings)
    int32_t musicVolume;
    int32_t minTimeStep; // min ms/frame; implicit fps cap (valid if hasSettings)
    int32_t hasSettings;
    // NOTE: vsync / gamma / gui-scale / target-fps are ABSENT in this build.
} CubeDisplay;

// Live audio state (client only, read-only). Edit volume via display settings (CUBE_DISPLAY_MUSIC_VOLUME / SOUND_VOLUME);
// no stored track name, and play/stop are game-calls not exposed here.
typedef struct CubeAudio
{
    uint32_t structSize;
    uint32_t address; // live XAudio2Engine pointer (0 if unavailable; for raw use)
    int32_t musicVolumeConfig; // saved music volume (options.cfg int, 0..100)
    int32_t soundVolumeConfig; // saved sfx volume int; NOTE: unused by the live mixer in this alpha
    int32_t musicPlaying; // 1 if the music streamer is currently playing (valid if hasMusicState)
    int32_t musicLooping; // 1 if the current music is looping (valid if hasMusicState)
    float musicVolumeLive; // the streamer's live volume float 0..1 (valid if hasMusicState)
    int32_t hasMusicState; // 1 if the engine -> music streamer chain resolved
} CubeAudio;

