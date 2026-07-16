#pragma once
// Sub-API vtable structs: one function-pointer table per domain.

#include "cube_sdk/types.h"
#include "cube_sdk/events_hooks.h"

typedef struct CubeLogApi
{
    void (CUBE_CALL* write)(const struct CubeApi* api, CubeLogLevel level, const char* message);
} CubeLogApi;

typedef struct CubeMemApi
{
    int32_t (CUBE_CALL* read)(const struct CubeApi* api, uint32_t address, void* out, uint32_t size);
    int32_t (CUBE_CALL* readable)(const struct CubeApi* api, uint32_t address, uint32_t size);
    uint32_t (CUBE_CALL* rebase)(const struct CubeApi* api, uint32_t staticAddress);
    // Guarded write of size bytes; validated first so a bad address fails (0) instead of crashing. Prefer the typed setters below.
    int32_t (CUBE_CALL* write)(const struct CubeApi* api, uint32_t address, const void* src, uint32_t size);
} CubeMemApi;

typedef struct CubeEventApi
{
    // Registers fn; returns a nonzero token (0 = bad args) to pass to unsubscribe.
    uint32_t (CUBE_CALL* subscribe)(const struct CubeApi* api, CubeEvent event, CubeEventFn fn, void* user);
    int32_t (CUBE_CALL* unsubscribe)(const struct CubeApi* api, uint32_t token);
} CubeEventApi;

typedef struct CubePlayerApi
{
    // Fills out with the local player; returns 1 on success, 0 if unavailable.
    int32_t (CUBE_CALL* get)(const struct CubeApi* api, CubePlayer* out);
    int32_t (CUBE_CALL* addXp)(const struct CubeApi* api, int32_t amount);
    int32_t (CUBE_CALL* teleport)(const struct CubeApi* api, float x, float y, float z);
    // Writes any CubePlayerStat at the field's correct width; value is a double covering float and int.
    int32_t (CUBE_CALL* setStat)(const struct CubeApi* api, int32_t stat, double value);
    // Sets the local player's name (narrow, truncated to the in-game buffer).
    int32_t (CUBE_CALL* setName)(const struct CubeApi* api, const char* name);
} CubePlayerApi;

typedef struct CubeCombatApi
{
    // Fills out with the local player's combat snapshot; 1 on success, 0 if unavailable.
    int32_t (CUBE_CALL* get)(const struct CubeApi* api, CubeCombat* out);
} CubeCombatApi;

typedef struct CubeItemsApi
{
    // Fills the local player's equipped items (present slots only) into out; returns count.
    int32_t (CUBE_CALL* equipment)(const struct CubeApi* api, CubeItem* out, int32_t maxCount);
    // Fills the local player's inventory items (non-empty cells) into out; returns count.
    int32_t (CUBE_CALL* inventory)(const struct CubeApi* api, CubeItem* out, int32_t maxCount);
    // Writes one CubeItemField of the item at itemAddress (from CubeItem.address); guarded. 1 on success.
    int32_t (CUBE_CALL* setField)(const struct CubeApi* api, uint32_t itemAddress, int32_t field, int32_t value);
    // Resolves an item (type, subtype) to its display name (the full item directory). Never null.
    const char* (CUBE_CALL* name)(const struct CubeApi* api, int32_t type, int32_t subtype);
    // Fills the inventory of ANY creature at address (e.g. a merchant's stock) into out; returns count.
    int32_t (CUBE_CALL* inventoryOf)(const struct CubeApi* api, uint32_t creatureAddress, CubeItem* out, int32_t maxCount);
    // The item's coin value (buy price); sell price is value/2. 0 if the item is unreadable.
    int32_t (CUBE_CALL* value)(const struct CubeApi* api, uint32_t itemAddress);
} CubeItemsApi;

// One live ability cooldown timer (only abilities used this session are present). Base durations are code formulas
// (not stored); only the live remainingMs is editable. `abilityId` matches CUBE_CATALOG_ABILITY.
typedef struct CubeAbilityCooldown
{
    uint32_t structSize;
    uint32_t address; // live map-node base pointer (for raw edits)
    int32_t abilityId;
    int32_t remainingMs;
} CubeAbilityCooldown;

typedef struct CubeSkillsApi
{
    // Fills the local player's skill ranks (points spent per skill) into out; returns count.
    int32_t (CUBE_CALL* ranks)(const struct CubeApi* api, int32_t* out, int32_t maxCount);
    // Sets skill rank [index] (0..CUBE_SKILL_COUNT-1) to value. 1 on success.
    int32_t (CUBE_CALL* setRank)(const struct CubeApi* api, int32_t index, int32_t value);
    // Fills the live per-ability cooldown timers into out; returns the count written.
    int32_t (CUBE_CALL* cooldowns)(const struct CubeApi* api, CubeAbilityCooldown* out, int32_t maxCount);
    // Sets abilityId's remaining cooldown (ms). 0 if the ability is not in the cooldown map yet or the player is unavailable.
    int32_t (CUBE_CALL* setCooldown)(const struct CubeApi* api, int32_t abilityId, int32_t remainingMs);
    // Zeroes every live ability cooldown (all abilities ready). Returns the count cleared.
    int32_t (CUBE_CALL* clearCooldowns)(const struct CubeApi* api);
} CubeSkillsApi;

typedef struct CubeStatusApi
{
    // Fills the local player's active status effects / buffs into out; returns count.
    int32_t (CUBE_CALL* effects)(const struct CubeApi* api, CubeBuff* out, int32_t maxCount);
    // Writes a CubeBuffField on the effect node at address (from CubeBuff.address). 1 on success.
    int32_t (CUBE_CALL* setField)(const struct CubeApi* api, uint32_t address, int32_t field, double value);
    // Fills the stun snapshot for the creature at address (0 = local player). 1 on success, 0 if unavailable.
    int32_t (CUBE_CALL* stun)(const struct CubeApi* api, uint32_t address, CubeStun* out);
    // Clears the stun on the creature at address (0 = local player): zeroes the timer and stands it up if knocked down.
    int32_t (CUBE_CALL* clearStun)(const struct CubeApi* api, uint32_t address);
} CubeStatusApi;

typedef struct CubeWorldApi
{
    int32_t (CUBE_CALL* get)(const struct CubeApi* api, CubeWorld* out);
    // Fills placed structures / POIs from the player's current zone; returns count.
    int32_t (CUBE_CALL* structures)(const struct CubeApi* api, CubeStructure* out, int32_t maxCount);
    // Sets the time-of-day (milliseconds in [0, 86400000); wrapped by the loader). 1 on success.
    int32_t (CUBE_CALL* setTime)(const struct CubeApi* api, int32_t ms);
    // Writes a CubeTileField on the player's current ZoneTile (terrain/temp/...). 1 on success.
    int32_t (CUBE_CALL* setTile)(const struct CubeApi* api, int32_t field, double value);
    // Sets the world generation seed. 1 on success.
    int32_t (CUBE_CALL* setSeed)(const struct CubeApi* api, uint32_t seed);
    // Sets the player's bound spawn point (world units). 1 on success.
    int32_t (CUBE_CALL* setSpawn)(const struct CubeApi* api, float x, float y, float z);
    // Writes a CubeStructureField on the structure at address (from CubeStructure.address). 1 on success.
    int32_t (CUBE_CALL* setStructure)(const struct CubeApi* api, uint32_t address, int32_t field, double value);
} CubeWorldApi;

typedef struct CubePetApi
{
    // Fills out with the local player's active pet; 1 if a live pet exists, else 0.
    int32_t (CUBE_CALL* get)(const struct CubeApi* api, CubePet* out);
} CubePetApi;

typedef struct CubeEntitiesApi
{
    // Fills up to maxCount nearby entities (excluding the local player); returns the number written.
    int32_t (CUBE_CALL* list)(const struct CubeApi* api, CubeEntity* out, int32_t maxCount);
    // Fills the currently selected/targeted entity; returns 1 if one exists.
    int32_t (CUBE_CALL* target)(const struct CubeApi* api, CubeEntity* out);
    // Fills the crosshair aim/hover target (distinct from the committed selection); 1 if it resolves to a creature.
    int32_t (CUBE_CALL* aimTarget)(const struct CubeApi* api, CubeEntity* out);
    // Fills the status effects of any creature at address (the status list is a generic Creature field); returns the count.
    int32_t (CUBE_CALL* effects)(const struct CubeApi* api, uint32_t address, CubeBuff* out, int32_t maxCount);
    // Writes a CubePlayerStat on the creature at address; the loader validates it is a Creature first. 1 on success.
    int32_t (CUBE_CALL* setStat)(const struct CubeApi* api, uint32_t address, int32_t stat, double value);
    // Sets the name of the creature at address (narrow, truncated). 1 on success.
    int32_t (CUBE_CALL* setName)(const struct CubeApi* api, uint32_t address, const char* name);
    // Moves the creature at address to (x, y, z) in world units. 1 on success.
    int32_t (CUBE_CALL* teleport)(const struct CubeApi* api, uint32_t address, float x, float y, float z);
    // 1 if the creature at address is a tameable passive critter (feed it a Food item to tame a pet).
    int32_t (CUBE_CALL* isTameable)(const struct CubeApi* api, uint32_t address);
} CubeEntitiesApi;

typedef struct CubeCameraApi
{
    int32_t (CUBE_CALL* get)(const struct CubeApi* api, CubeCamera* out); // client only
    // Writes a CubeCameraField (distance/pitch/yaw). 1 on success. client only.
    int32_t (CUBE_CALL* set)(const struct CubeApi* api, int32_t field, double value);
} CubeCameraApi;

typedef struct CubeDisplayApi
{
    int32_t (CUBE_CALL* get)(const struct CubeApi* api, CubeDisplay* out); // client only
    // Writes a CubeDisplayField (fullscreen/resolution/volumes/...). 1 on success. client only.
    int32_t (CUBE_CALL* set)(const struct CubeApi* api, int32_t field, int32_t value);
} CubeDisplayApi;

typedef struct CubeAudioApi
{
    int32_t (CUBE_CALL* get)(const struct CubeApi* api, CubeAudio* out); // client only
    // Play a built-in sound effect (CUBE_CATALOG_SOUND id, 0..100) 2D at the listener. 1 on success.
    // WARNING: game-call; invoke from the game thread (an event callback / hook), not another thread.
    int32_t (CUBE_CALL* playSound)(const struct CubeApi* api, int32_t soundId);
    // Play a built-in sound effect at a world position with volume/pitch multipliers. 1 on success.
    int32_t (CUBE_CALL* playSoundAt)(const struct CubeApi* api, int32_t soundId, float x, float y, float z, float volume, float pitch);
    // Stop the currently playing music. 1 on success. (Playing music by name needs a file path the
    // game does not expose, so it is not offered here.)
    int32_t (CUBE_CALL* stopMusic)(const struct CubeApi* api);
    // Set the live music volume (0..1) on the audio engine. 1 on success.
    int32_t (CUBE_CALL* setMusicVolume)(const struct CubeApi* api, float volume);
} CubeAudioApi;

// Where the game is (resolved by the loader from the local player + GC).
typedef enum CubeGameState
{
    CUBE_STATE_UNKNOWN = 0,
    CUBE_STATE_TITLE, // no world / at the menu
    CUBE_STATE_LOADING,
    CUBE_STATE_IN_WORLD
} CubeGameState;

typedef enum CubeNetworkMode
{
    CUBE_NET_UNKNOWN = 0,
    CUBE_NET_SINGLEPLAYER,
    CUBE_NET_MULTIPLAYER
} CubeNetworkMode;

typedef struct CubeSession
{
    uint32_t structSize;
    uint32_t address; // live GameController base pointer (0 if unavailable; for raw use)
    int32_t gameState; // CubeGameState
    int32_t networkMode; // CubeNetworkMode (valid if hasNetwork)
    int32_t connected; // 1 if networking is active (valid if hasNetwork)
    int32_t playerCount; // players in the session (>=1 in world)
    int32_t hasNetwork; // 1 if the network fields were resolved
} CubeSession;

// Which HUD menus are open, so a mod knows not to grab input. `anyOpen` is the
// consolidated "the player is in a menu" flag.
typedef struct CubeUi
{
    uint32_t structSize;
    uint32_t address; // live GameController base pointer (0 if unavailable; for raw use)
    int32_t inventoryOpen;
    int32_t characterOpen;
    int32_t mapOpen;
    int32_t objectiveOpen;
    int32_t anyOpen;
    int32_t valid;
} CubeUi;

typedef struct CubeSessionApi
{
    int32_t (CUBE_CALL* get)(const struct CubeApi* api, CubeSession* out);
    // Writes a CubeSessionField (network mode / connected). 1 on success.
    int32_t (CUBE_CALL* setField)(const struct CubeApi* api, int32_t field, int32_t value);
} CubeSessionApi;

typedef struct CubeUiApi
{
    int32_t (CUBE_CALL* get)(const struct CubeApi* api, CubeUi* out); // client only
    // Forces a CubeUiField open/closed (client only); input-driven panels may be re-closed by the game next frame. 1 on success.
    int32_t (CUBE_CALL* setField)(const struct CubeApi* api, int32_t field, int32_t open);
} CubeUiApi;

// Human-readable name catalogs, so a mod can render/edit opaque game ids (item
// type, material, terrain, buff, ...) as names + dropdowns instead of raw numbers.
typedef enum CubeCatalog
{
    CUBE_CATALOG_ITEM_TYPE = 0,
    CUBE_CATALOG_WEAPON_SUBTYPE,
    CUBE_CATALOG_MATERIAL,
    CUBE_CATALOG_ITEM_MODIFIER,
    CUBE_CATALOG_TERRAIN,
    CUBE_CATALOG_BUFF_TYPE,
    CUBE_CATALOG_ACTION,
    CUBE_CATALOG_STRUCTURE_TYPE,
    CUBE_CATALOG_ENTITY_CATEGORY,
    CUBE_CATALOG_CLASS,
    CUBE_CATALOG_SPECIES,
    CUBE_CATALOG_SKILL, // indexed by skill-array slot (0..CUBE_SKILL_COUNT-1)
    CUBE_CATALOG_ABILITY, // ability id -> name (matches CubeAbilityCooldown.abilityId)
    CUBE_CATALOG_CONSUMABLE_SUBTYPE, // item subtype when type == Consumable (1): potions/edibles
    CUBE_CATALOG_FOOD_SUBTYPE, // item subtype when type == Food (20); only named overrides (default = Bait)
    CUBE_CATALOG_SPECIAL_SUBTYPE, // item subtype when type == Special (11): crafting materials / parts
    CUBE_CATALOG_ACCESSORY_SUBTYPE, // item subtype when type == Accessory (21): medical / trinkets
    CUBE_CATALOG_VEHICLE_SUBTYPE, // item subtype when type == Vehicle (23)
    CUBE_CATALOG_SOUND, // built-in sound-effect id -> wav name (for audio.playSound)
    CUBE_CATALOG_COUNT
} CubeCatalog;

// One (id, name) entry of a catalog. `name` is a static string owned by the loader.
typedef struct CubeNamedValue
{
    int32_t id;
    const char* name;
} CubeNamedValue;

typedef struct CubeCatalogApi
{
    // Fills up to maxCount (id,name) entries of the catalog into out; returns the count.
    int32_t (CUBE_CALL* list)(const struct CubeApi* api, int32_t catalog, CubeNamedValue* out, int32_t maxCount);
    // Returns the name for id within catalog, or NULL if unmapped (static string).
    const char* (CUBE_CALL* name)(const struct CubeApi* api, int32_t catalog, int32_t id);
} CubeCatalogApi;

// Input control (client only). Movement/look come through DirectInput, so swallowing window messages cannot stop
// the player; setBlocked(1) zeroes DI state and freezes the cursor recenter (movement + camera halt), setBlocked(0) restores.
typedef struct CubeInputApi
{
    int32_t (CUBE_CALL* setBlocked)(const struct CubeApi* api, int32_t blocked);
} CubeInputApi;

// Selection (client only). The loader detours the R / use key (GameController::updateSelectedEntity);
// subscribe to CUBE_EVENT_ENTITY_SELECTED or poll getLast().
typedef struct CubeSelectionApi
{
    // Fills the most recent selection; returns 1 if any selection has happened, else 0.
    int32_t (CUBE_CALL* getLast)(const struct CubeApi* api, CubeSelection* out);
} CubeSelectionApi;

// Item pickup (client only). The loader detours the E / hold-to-pickup action
// (GameController::onItemPickup); subscribe to CUBE_EVENT_ITEM_PICKUP or poll getLast().
typedef struct CubePickupApi
{
    // Fills the most recently picked-up item (out.stack = count); returns 1 if any pickup has
    // happened, else 0. out.address is 0 (the picked item is a transient staging copy).
    int32_t (CUBE_CALL* getLast)(const struct CubeApi* api, CubeItem* out);
} CubePickupApi;

// Game-function hooking (interception). All registrations are per-mod and auto-removed on unload; see the game-thread warning above.
typedef struct CubeHooksApi
{
    // Built-in (semantic) hook: run fn whenever the named game function is called. Returns 1.
    int32_t (CUBE_CALL* on)(const struct CubeApi* api, CubeHook hook, CubeHookFn fn, void* user);
    // Detach all of this mod's handlers for a built-in hook. Returns 1 if any were removed.
    int32_t (CUBE_CALL* off)(const struct CubeApi* api, CubeHook hook);
    // Raw hook of an address the mod found itself; fn receives a CubeHookCall via a generic capture stub.
    // First cut: __thiscall/__cdecl, up to CUBE_HOOK_ARG_MAX int/pointer args, int return. 0 if the pool is full or unhookable.
    int32_t (CUBE_CALL* onRaw)(const struct CubeApi* api, uint32_t address, CubeCallConv cc,
                               int32_t argCount, CubeHookFn fn, void* user);
    // Escape hatch: install a mod-supplied detour and receive the trampoline; the mod owns the signature/convention.
    int32_t (CUBE_CALL* installRawDetour)(const struct CubeApi* api, uint32_t address, void* detour,
                                          void** trampoline);
    // Remove any raw hook (onRaw or installRawDetour) this mod placed at address. Returns 1/0.
    int32_t (CUBE_CALL* removeRaw)(const struct CubeApi* api, uint32_t address);
    // Runtime (rebased) address of a built-in hook's target game function, or 0 if unknown. Lets a mod
    // coordinate a raw hook with a built-in without hardcoding a loader-owned address.
    uint32_t (CUBE_CALL* builtinTarget)(const struct CubeApi* api, int32_t hook);
} CubeHooksApi;

// Per-mod user-editable settings, persisted to <dllDir>/config/<mod-id>.ini (keyed by mod id, resolved
// loader-side - no id argument). Each getter takes a fallback returned when the key is missing/malformed.
typedef struct CubeConfigApi
{
    int32_t (CUBE_CALL* getInt)(const struct CubeApi* api, const char* key, int32_t fallback);
    double (CUBE_CALL* getFloat)(const struct CubeApi* api, const char* key, double fallback);
    int32_t (CUBE_CALL* getBool)(const struct CubeApi* api, const char* key, int32_t fallback);
    // Copies the value (or fallback) into out (always null-terminated within size); returns the string length.
    int32_t (CUBE_CALL* getString)(const struct CubeApi* api, const char* key, const char* fallback, char* out, int32_t size);
    int32_t (CUBE_CALL* setInt)(const struct CubeApi* api, const char* key, int32_t value);
    int32_t (CUBE_CALL* setFloat)(const struct CubeApi* api, const char* key, double value);
    int32_t (CUBE_CALL* setBool)(const struct CubeApi* api, const char* key, int32_t value);
    int32_t (CUBE_CALL* setString)(const struct CubeApi* api, const char* key, const char* value);
} CubeConfigApi;

// Per-mod save data: opaque binary blobs under <dllDir>/data/<mod-id>/ (keyed by mod id, resolved
// loader-side). Distinct from config (that is user-editable text); this is mod-owned, binary-safe state.
typedef struct CubeStorageApi
{
    // Namespace subsequent get/put/... under a scope subdirectory (e.g. world seed / character). "" or NULL = the unscoped root. Returns 1.
    int32_t (CUBE_CALL* setScope)(const struct CubeApi* api, const char* scope);
    // Copies up to size bytes of the blob at key into out; returns bytes read (0 if absent). out == NULL probes: returns the stored size.
    int32_t (CUBE_CALL* get)(const struct CubeApi* api, const char* key, void* out, int32_t size);
    // Writes size bytes as the blob at key (overwrites). Returns 1 on success.
    int32_t (CUBE_CALL* put)(const struct CubeApi* api, const char* key, const void* data, int32_t size);
    // Deletes the blob at key. Returns 1 if it existed and was removed.
    int32_t (CUBE_CALL* remove)(const struct CubeApi* api, const char* key);
    // 1 if a blob exists at key.
    int32_t (CUBE_CALL* has)(const struct CubeApi* api, const char* key);
} CubeStorageApi;

// Inter-mod ecosystem: a mod publishes a named, versioned shared service (any consumer resolves it by
// name at CUBE_EVENT_READY) and/or receives directed messages addressed to its manifest id. The service
// impl pointer and message payloads cross the DLL boundary raw - provider and consumer own their
// lifetime and agree their layout by contract (the loader never dereferences either).
typedef struct CubeServicesApi
{
    // Publish impl under name at version (>=1). Re-registering the same name from the same mod replaces
    // it. Returns 1 on success, 0 on bad args.
    int32_t (CUBE_CALL* registerService)(const struct CubeApi* api, const char* name, uint32_t version, void* impl);
    // Withdraw this mod's service named name. Returns 1 if one existed. (Also dropped automatically on unload.)
    int32_t (CUBE_CALL* unregisterService)(const struct CubeApi* api, const char* name);
    // Resolve the highest-version provider of name whose version >= minVersion; NULL if none. Optionally
    // writes the chosen provider's version to outVersion (may be NULL).
    void* (CUBE_CALL* query)(const struct CubeApi* api, const char* name, uint32_t minVersion, uint32_t* outVersion);
    // Register this mod's message receiver. Returns a token (0 on failure); pass it to clearMessageHandler.
    uint32_t (CUBE_CALL* onMessage)(const struct CubeApi* api, CubeMessageFn fn, void* user);
    // Remove a receiver installed by onMessage. Returns 1 if it existed. (Also dropped on unload.)
    int32_t (CUBE_CALL* clearMessageHandler)(const struct CubeApi* api, uint32_t token);
    // Send msgId + optional payload to the mod whose manifest id == targetModId. Returns the receiving
    // handler's CubeMessageArgs.result, or -1 if the target id is unknown or has no message handler.
    int32_t (CUBE_CALL* sendMessage)(const struct CubeApi* api, const char* targetModId, uint32_t msgId, void* payload, uint32_t payloadSize);
} CubeServicesApi;

