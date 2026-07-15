#pragma once
// Shared value types for the cube:: SDK: enums, Vec3, NamedValue, the catalog namespace,
// and the enum-name helpers. Every other cube/ header includes this one.

#include "cube_sdk.h"

#include <cstdarg>
#include <cstdio>
#include <functional>
#include <map>
#include <utility>
#include <vector>

namespace cube
{
    namespace detail
    {
        // Fills a fixed-max C buffer via one list() call and wraps each element; shared by the
        // equipmentOf / inventoryOf / buffsOf / abilityCooldownsOf / structuresOf collection helpers.
        template <typename CObj, typename Wrapper, size_t Max>
        inline std::vector<Wrapper> fillList(const CubeApi* api, int32_t (CUBE_CALL* list)(const CubeApi*, CObj*, int32_t))
        {
            std::vector<Wrapper> result;
            if (!api || !list)
                return result;
            CObj buffer[Max];
            const int32_t count = list(api, buffer, static_cast<int32_t>(Max));
            result.reserve(static_cast<size_t>(count < 0 ? 0 : count));
            for (int32_t i = 0; i < count; ++i)
                result.push_back(Wrapper(buffer[static_cast<size_t>(i)], api));
            return result;
        }
    }

    enum class Event
    {
        Startup = CUBE_EVENT_STARTUP,
        Shutdown = CUBE_EVENT_SHUTDOWN,
        Frame = CUBE_EVENT_FRAME,
        DeviceReset = CUBE_EVENT_DEVICE_RESET,
        WndProc = CUBE_EVENT_WNDPROC,
        Attack = CUBE_EVENT_PLAYER_ATTACK,
        Jump = CUBE_EVENT_PLAYER_JUMP,
        AreaChange = CUBE_EVENT_AREA_CHANGE,
        Damaged = CUBE_EVENT_PLAYER_DAMAGED,
        EntityDamaged = CUBE_EVENT_ENTITY_DAMAGED,
        Crit = CUBE_EVENT_PLAYER_CRIT,
        MenuOpen = CUBE_EVENT_MENU_OPEN,
        MenuClose = CUBE_EVENT_MENU_CLOSE,
        LevelUp = CUBE_EVENT_PLAYER_LEVELUP,
        Death = CUBE_EVENT_PLAYER_DEATH,
        Respawn = CUBE_EVENT_PLAYER_RESPAWN,
        Land = CUBE_EVENT_PLAYER_LAND,
        MovementChanged = CUBE_EVENT_MOVEMENT_CHANGED,
        TargetChanged = CUBE_EVENT_TARGET_CHANGED,
        EntitySpawn = CUBE_EVENT_ENTITY_SPAWN,
        EntityDeath = CUBE_EVENT_ENTITY_DEATH,
        CoinsChanged = CUBE_EVENT_COINS_CHANGED,
        DayNight = CUBE_EVENT_DAY_NIGHT,
        BuffGained = CUBE_EVENT_BUFF_GAINED,
        BuffLost = CUBE_EVENT_BUFF_LOST,
        EquipmentChanged = CUBE_EVENT_EQUIPMENT_CHANGED,
        SkillRankChanged = CUBE_EVENT_SKILL_RANK_CHANGED,
        AimTargetChanged = CUBE_EVENT_AIM_TARGET_CHANGED,
        EntityDespawn = CUBE_EVENT_ENTITY_DESPAWN,
        PetSummoned = CUBE_EVENT_PET_SUMMONED,
        PetDied = CUBE_EVENT_PET_DIED,
        PetDismissed = CUBE_EVENT_PET_DISMISSED,
        Stunned = CUBE_EVENT_PLAYER_STUNNED,
        KnockedDown = CUBE_EVENT_PLAYER_KNOCKED_DOWN,
        Recovered = CUBE_EVENT_PLAYER_RECOVERED,
        EntityStunned = CUBE_EVENT_ENTITY_STUNNED,
        EntityKnockedDown = CUBE_EVENT_ENTITY_KNOCKED_DOWN,
        PetStunned = CUBE_EVENT_PET_STUNNED,
        PetKnockedDown = CUBE_EVENT_PET_KNOCKED_DOWN,
        EntitySelected = CUBE_EVENT_ENTITY_SELECTED,
        EntityRecovered = CUBE_EVENT_ENTITY_RECOVERED,
        PetRecovered = CUBE_EVENT_PET_RECOVERED,
        AbilityUsed = CUBE_EVENT_ABILITY_USED,
        ItemPickup = CUBE_EVENT_ITEM_PICKUP
    };

    enum class SelectionKind
    {
        None = CUBE_SELECTION_NONE,
        Creature = CUBE_SELECTION_CREATURE,
        Container = CUBE_SELECTION_CONTAINER,
        Dialog = CUBE_SELECTION_DIALOG,
        Widget = CUBE_SELECTION_WIDGET,
        World = CUBE_SELECTION_WORLD
    };

    inline const char* selectionKindName(SelectionKind kind)
    {
        switch (kind)
        {
            case SelectionKind::Creature: return "creature";
            case SelectionKind::Container: return "container";
            case SelectionKind::Dialog: return "dialog";
            case SelectionKind::Widget: return "widget";
            case SelectionKind::World: return "world";
            default: return "none";
        }
    }

    enum class GameState
    {
        Unknown = CUBE_STATE_UNKNOWN,
        Title = CUBE_STATE_TITLE,
        Loading = CUBE_STATE_LOADING,
        InWorld = CUBE_STATE_IN_WORLD
    };

    enum class NetworkMode
    {
        Unknown = CUBE_NET_UNKNOWN,
        Singleplayer = CUBE_NET_SINGLEPLAYER,
        Multiplayer = CUBE_NET_MULTIPLAYER
    };

    // Built-in game-function hook (interception): the handler gets a HookCall to cancel/mutate/override.
    enum class Hook
    {
        Impact = CUBE_HOOK_IMPACT, // an incoming hit lands; cancel() to negate it, argInt(0)=damage
        CritRoll = CUBE_HOOK_CRIT_ROLL, // a crit roll; setReturnInt(1/0) to force/deny it
        MaxHealth = CUBE_HOOK_MAX_HEALTH // a creature's max HP is computed; setReturnFloat to rescale
    };

    // Calling convention of a raw hook target, so the loader picks the matching capture stub.
    enum class CallConv
    {
        Thiscall = CUBE_CC_THISCALL,
        Cdecl = CUBE_CC_CDECL,
        Stdcall = CUBE_CC_STDCALL,
        Fastcall = CUBE_CC_FASTCALL
    };

    enum class Class
    {
        Unknown = CUBE_CLASS_UNKNOWN,
        Warrior = CUBE_CLASS_WARRIOR,
        Ranger = CUBE_CLASS_RANGER,
        Mage = CUBE_CLASS_MAGE,
        Rogue = CUBE_CLASS_ROGUE
    };

    // Consolidated locomotion state: loader resolves it once, mod reads one value (not scattered booleans).
    enum class Movement
    {
        Unknown = CUBE_MOVE_UNKNOWN,
        Grounded = CUBE_MOVE_GROUNDED,
        Airborne = CUBE_MOVE_AIRBORNE,
        Climbing = CUBE_MOVE_CLIMBING,
        Swimming = CUBE_MOVE_SWIMMING,
        Gliding = CUBE_MOVE_GLIDING,
        Sneaking = CUBE_MOVE_SNEAKING,
        Sitting = CUBE_MOVE_SITTING
    };

    enum class Action
    {
        Unknown = CUBE_ACTION_UNKNOWN,
        Idle = CUBE_ACTION_IDLE,
        Attacking = CUBE_ACTION_ATTACKING,
        Casting = CUBE_ACTION_CASTING,
        Blocking = CUBE_ACTION_BLOCKING,
        Rolling = CUBE_ACTION_ROLLING,
        Dead = CUBE_ACTION_DEAD
    };

    inline const char* movementName(Movement m)
    {
        switch (m)
        {
            case Movement::Grounded: return "grounded";
            case Movement::Airborne: return "airborne";
            case Movement::Climbing: return "climbing";
            case Movement::Swimming: return "swimming";
            case Movement::Gliding: return "gliding";
            case Movement::Sneaking: return "sneaking";
            case Movement::Sitting: return "sitting";
            default: return "unknown";
        }
    }

    inline const char* actionName(Action a)
    {
        switch (a)
        {
            case Action::Idle: return "idle";
            case Action::Attacking: return "attacking";
            case Action::Casting: return "casting";
            case Action::Blocking: return "blocking";
            case Action::Rolling: return "rolling";
            case Action::Dead: return "dead";
            default: return "unknown";
        }
    }

    enum class Environment
    {
        Client = CUBE_ENV_CLIENT,
        Server = CUBE_ENV_SERVER
    };

    enum class Relation
    {
        Unknown = CUBE_REL_UNKNOWN,
        Self = CUBE_REL_SELF,
        OwnPet = CUBE_REL_OWN_PET,
        Player = CUBE_REL_PLAYER,
        Npc = CUBE_REL_NPC,
        Neutral = CUBE_REL_NEUTRAL,
        Hostile = CUBE_REL_HOSTILE
    };

    enum class EntityState
    {
        Unknown = CUBE_ENTSTATE_UNKNOWN,
        Alive = CUBE_ENTSTATE_ALIVE,
        Dying = CUBE_ENTSTATE_DYING,
        Dead = CUBE_ENTSTATE_DEAD
    };

    inline const char* relationName(Relation r)
    {
        switch (r)
        {
            case Relation::Self: return "self";
            case Relation::OwnPet: return "pet";
            case Relation::Player: return "player";
            case Relation::Npc: return "npc";
            case Relation::Neutral: return "neutral";
            case Relation::Hostile: return "hostile";
            default: return "unknown";
        }
    }

    inline const char* entityStateName(EntityState state)
    {
        switch (state)
        {
            case EntityState::Alive: return "alive";
            case EntityState::Dying: return "dying";
            case EntityState::Dead: return "dead";
            default: return "unknown";
        }
    }

    // Wrapped mirrors of the C stat enums (same values) so a mod never types a CUBE_* macro.
    enum class PlayerStat
    {
        Health = CUBE_STAT_HEALTH,
        Mana = CUBE_STAT_MANA,
        Stamina = CUBE_STAT_STAMINA,
        Coins = CUBE_STAT_COINS,
        Level = CUBE_STAT_LEVEL,
        Xp = CUBE_STAT_XP,
        Class = CUBE_STAT_CLASS,
        Spec = CUBE_STAT_SPEC,
        Type = CUBE_STAT_TYPE,
        Facing = CUBE_STAT_FACING,
        Power = CUBE_STAT_POWER,
        Armor = CUBE_STAT_ARMOR,
        Spirit = CUBE_STAT_SPIRIT,
        Combo = CUBE_STAT_COMBO,
        AttackCooldown = CUBE_STAT_ATTACK_COOLDOWN,
        HitStun = CUBE_STAT_HITSTUN,
        VelX = CUBE_STAT_VEL_X,
        VelY = CUBE_STAT_VEL_Y,
        VelZ = CUBE_STAT_VEL_Z,
        ActionId = CUBE_STAT_ACTION_ID,
        Category = CUBE_STAT_CATEGORY,
        Rank = CUBE_STAT_RANK,
        AttackSpeed = CUBE_STAT_ATTACK_SPEED,
        Crit = CUBE_STAT_CRIT,
        Sneaking = CUBE_STAT_SNEAKING,
        BaseDamage = CUBE_STAT_BASE_DAMAGE,
        Lantern = CUBE_STAT_LANTERN
    };

    enum class ItemField
    {
        Type = CUBE_ITEM_FIELD_TYPE,
        Subtype = CUBE_ITEM_FIELD_SUBTYPE,
        Material = CUBE_ITEM_FIELD_MATERIAL,
        Modifier = CUBE_ITEM_FIELD_MODIFIER,
        Level = CUBE_ITEM_FIELD_LEVEL,
        UpgradeCount = CUBE_ITEM_FIELD_UPGRADE_COUNT,
        Seed = CUBE_ITEM_FIELD_SEED,
        Stack = CUBE_ITEM_FIELD_STACK
    };

    enum class TileField
    {
        Terrain = CUBE_TILE_TERRAIN,
        Temperature = CUBE_TILE_TEMPERATURE,
        Humidity = CUBE_TILE_HUMIDITY,
        Elevation = CUBE_TILE_ELEVATION
    };

    enum class StructField
    {
        Type = CUBE_STRUCT_TYPE,
        PosX = CUBE_STRUCT_POS_X,
        PosY = CUBE_STRUCT_POS_Y,
        PosZ = CUBE_STRUCT_POS_Z
    };

    enum class BuffField
    {
        Type = CUBE_BUFF_TYPE,
        Magnitude = CUBE_BUFF_MAGNITUDE,
        Duration = CUBE_BUFF_DURATION
    };

    enum class CameraField
    {
        Distance = CUBE_CAM_DISTANCE,
        Pitch = CUBE_CAM_PITCH,
        Yaw = CUBE_CAM_YAW
    };

    enum class DisplayField
    {
        Fullscreen = CUBE_DISPLAY_FULLSCREEN,
        ResolutionX = CUBE_DISPLAY_RESOLUTION_X,
        ResolutionY = CUBE_DISPLAY_RESOLUTION_Y,
        RenderDistance = CUBE_DISPLAY_RENDER_DISTANCE,
        SoundVolume = CUBE_DISPLAY_SOUND_VOLUME,
        MusicVolume = CUBE_DISPLAY_MUSIC_VOLUME,
        MinTimeStep = CUBE_DISPLAY_MIN_TIMESTEP
    };

    enum class SessionField
    {
        NetworkMode = CUBE_SESSION_NETWORK_MODE,
        Connected = CUBE_SESSION_CONNECTED
    };

    enum class UiField
    {
        Inventory = CUBE_UI_INVENTORY,
        Character = CUBE_UI_CHARACTER,
        Map = CUBE_UI_MAP,
        Objective = CUBE_UI_OBJECTIVE
    };

    struct Vec3
    {
        float x;
        float y;
        float z;
    };

    typedef CubeEventArgs EventArgs;

    // An opaque game id paired with its human-readable name.
    struct NamedValue
    {
        int id;
        const char* name;
    };

    // Turn opaque game ids into names and dropdown option lists.
    namespace catalog
    {

        inline const char* name(const CubeApi* api, CubeCatalog which, int id)
        {
            return (api && api->catalog.name) ? api->catalog.name(api, static_cast<int32_t>(which), id) : nullptr;
        }

        inline const char* nameOr(const CubeApi* api, CubeCatalog which, int id, const char* fallback)
        {
            const char* resolved = name(api, which, id);
            return resolved ? resolved : fallback;
        }

        inline std::vector<NamedValue> options(const CubeApi* api, CubeCatalog which)
        {
            std::vector<NamedValue> result;
            if (!api || !api->catalog.list)
                return result;
            CubeNamedValue buffer[CUBE_CATALOG_MAX];
            const int32_t count = api->catalog.list(api, static_cast<int32_t>(which), buffer, CUBE_CATALOG_MAX);
            result.reserve(static_cast<size_t>(count));
            for (int32_t i = 0; i < count; ++i)
                result.push_back(NamedValue{buffer[i].id, buffer[i].name});
            return result;
        }

    }
}
