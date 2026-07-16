#pragma once
// Event subscription facades: EventListener (observe) + EventHook (intercept).

#include "cube/common.hpp"
#include "cube/items.hpp"
#include "cube/hookcall.hpp"

namespace cube
{
    class Mod;

    // OBSERVE. Handlers run AFTER the loader detects a game change; read-only, the game runs normally.
    // The counterpart is EventHook (INTERCEPT).
    class EventListener
    {
    public:
        explicit EventListener(Mod* owner) : m_mod(owner) {}

        // Generic forms: no-arg on() for the common case; onRaw() hands you the full EventArgs.
        void on(Event event, std::function<void()> fn);
        void onRaw(Event event, std::function<void(EventArgs&)> fn);
        // Cancel every handler this mod registered for an event. Returns true if it was subscribed.
        bool remove(Event event);

        void onFrame(std::function<void()> fn) { on(Event::Frame, std::move(fn)); }
        // Fires once per PRIMARY attack (each left-click swing / shot), including every combo step and
        // repeated shot. Hotbar abilities are NOT this event - use onAbilityUsed for those.
        void onAttack(std::function<void()> fn) { on(Event::Attack, std::move(fn)); }
        // Payload: the raw action id at the swing (the attack action byte), and the selected target
        // address at the edge (0 if none; the selected target, not a confirmed victim).
        void onAttack(std::function<void(int actionId)> fn) { onRaw(Event::Attack, [fn = std::move(fn)](EventArgs& a) { fn(a.param); }); }
        void onAttack(std::function<void(int actionId, unsigned selectedTarget)> fn) { onRaw(Event::Attack, [fn = std::move(fn)](EventArgs& a) { fn(a.param, static_cast<unsigned>(a.param2)); }); }
        // Fires once each time the local player uses a hotbar ability (keys 1-5).
        void onAbilityUsed(std::function<void()> fn) { on(Event::AbilityUsed, std::move(fn)); }
        // Payload: the ability id (matches CUBE_CATALOG_ABILITY), and the cooldown ms the use set.
        void onAbilityUsed(std::function<void(int abilityId)> fn) { onRaw(Event::AbilityUsed, [fn = std::move(fn)](EventArgs& a) { fn(a.param); }); }
        void onAbilityUsed(std::function<void(int abilityId, int cooldownMs)> fn) { onRaw(Event::AbilityUsed, [fn = std::move(fn)](EventArgs& a) { fn(a.param, a.param2); }); }
        void onJump(std::function<void()> fn) { on(Event::Jump, std::move(fn)); }
        // Payload: vertical velocity (velZ) the game had at the jump edge.
        void onJump(std::function<void(float verticalVelocity)> fn) { onRaw(Event::Jump, [fn = std::move(fn)](EventArgs& a) { fn(a.amount); }); }
        void onAreaChange(std::function<void()> fn) { on(Event::AreaChange, std::move(fn)); }
        // Payload: the new zone coordinates the player entered.
        void onAreaChange(std::function<void(int zoneX, int zoneY)> fn) { onRaw(Event::AreaChange, [fn = std::move(fn)](EventArgs& a) { fn(a.param, a.param2); }); }
        void onDamaged(std::function<void()> fn) { on(Event::Damaged, std::move(fn)); }
        // Fires when ANY nearby creature takes damage (not necessarily player-caused). For the player's
        // own attributed hits use eventHook().onImpact. (Renamed from onHit, which over-promised.)
        void onEntityDamaged(std::function<void()> fn) { on(Event::EntityDamaged, std::move(fn)); }
        void onCrit(std::function<void()> fn) { on(Event::Crit, std::move(fn)); }
        // Payload: `damage` is HP LOST this frame (an HP delta the loader observes by diffing health
        // frame-to-frame) - it is post-mitigation and NOT the game's raw damage value, and the cause is
        // not attributed (any HP loss, whoever caused it). `remainingHealth` is the victim's HP after
        // the loss. Filter by victim against an address you hold (a tracked entity, or your target via
        // target(Entity&) -> getAddress()). For the game's actual damage argument and the attacker (and
        // to cancel/rescale the hit) use mod.eventHook().onImpact instead.
        void onDamaged(std::function<void(float damage)> fn) { onRaw(Event::Damaged, [fn = std::move(fn)](EventArgs& a) { fn(a.amount); }); }
        void onDamaged(std::function<void(float damage, int remainingHealth)> fn) { onRaw(Event::Damaged, [fn = std::move(fn)](EventArgs& a) { fn(a.amount, a.param2); }); }
        void onEntityDamaged(std::function<void(unsigned victim, float damage)> fn) { onRaw(Event::EntityDamaged, [fn = std::move(fn)](EventArgs& a) { fn(a.subject, a.amount); }); }
        void onEntityDamaged(std::function<void(unsigned victim, float damage, int remainingHealth)> fn) { onRaw(Event::EntityDamaged, [fn = std::move(fn)](EventArgs& a) { fn(a.subject, a.amount, a.param2); }); }
        // Fullest form: also the victim's category byte (monster/player/pet/npc), so you can filter
        // "only count damage to monsters" without a second lookup.
        void onEntityDamaged(std::function<void(unsigned victim, float damage, int remainingHealth, int category)> fn) { onRaw(Event::EntityDamaged, [fn = std::move(fn)](EventArgs& a) { fn(a.subject, a.amount, a.param2, a.param); }); }
        void onMenuOpen(std::function<void()> fn) { on(Event::MenuOpen, std::move(fn)); }
        // Payload: a CubeUiMenuMask bitmask of which tracked HUD panels are open.
        void onMenuOpen(std::function<void(int openMask)> fn) { onRaw(Event::MenuOpen, [fn = std::move(fn)](EventArgs& a) { fn(a.param); }); }
        void onMenuClose(std::function<void()> fn) { on(Event::MenuClose, std::move(fn)); }
        void onMenuClose(std::function<void(int openMask)> fn) { onRaw(Event::MenuClose, [fn = std::move(fn)](EventArgs& a) { fn(a.param); }); }
        void onLevelUp(std::function<void()> fn) { on(Event::LevelUp, std::move(fn)); }
        void onLevelUp(std::function<void(int newLevel)> fn) { onRaw(Event::LevelUp, [fn = std::move(fn)](EventArgs& a) { fn(a.param); }); }
        void onLevelUp(std::function<void(int newLevel, int previousLevel)> fn) { onRaw(Event::LevelUp, [fn = std::move(fn)](EventArgs& a) { fn(a.param, a.param2); }); }
        void onUnload(std::function<void()> fn) { on(Event::Shutdown, std::move(fn)); }
        // Fires once after every mod has loaded and passed dependency resolution: the safe point to
        // resolve another mod's registered service (mod.services().query). Runs on the load thread.
        void onReady(std::function<void()> fn) { on(Event::Ready, std::move(fn)); }
        // The local player became resident in / left a world (title/menu <-> in-world). Distinct from
        // onAreaChange, which fires on zone-to-zone crossings while already in a world.
        void onWorldEnter(std::function<void()> fn) { on(Event::WorldEnter, std::move(fn)); }
        void onWorldExit(std::function<void()> fn) { on(Event::WorldExit, std::move(fn)); }
        void onDeath(std::function<void()> fn) { on(Event::Death, std::move(fn)); }
        void onRespawn(std::function<void()> fn) { on(Event::Respawn, std::move(fn)); }
        void onLand(std::function<void()> fn) { on(Event::Land, std::move(fn)); }
        // Payload: vertical velocity (velZ) the game had at the landing edge.
        void onLand(std::function<void(float verticalVelocity)> fn) { onRaw(Event::Land, [fn = std::move(fn)](EventArgs& a) { fn(a.amount); }); }
        // A dodge-roll (distinct from a jump: the roll's upward pop no longer fires onJump/onStunned).
        void onRoll(std::function<void()> fn) { on(Event::Roll, std::move(fn)); }
        void onRoll(std::function<void(float verticalVelocity)> fn) { onRaw(Event::Roll, [fn = std::move(fn)](EventArgs& a) { fn(a.amount); }); }
        // Payload-carrying events pass the decoded value to the handler; use onRaw() for the full EventArgs.
        void onMovementChanged(std::function<void(Movement)> fn) { onRaw(Event::MovementChanged, [fn = std::move(fn)](EventArgs& a) { fn(static_cast<Movement>(a.param)); }); }
        void onMovementChanged(std::function<void(Movement current, Movement previous)> fn) { onRaw(Event::MovementChanged, [fn = std::move(fn)](EventArgs& a) { fn(static_cast<Movement>(a.param), static_cast<Movement>(a.param2)); }); }
        void onTargetChanged(std::function<void(unsigned)> fn) { onRaw(Event::TargetChanged, [fn = std::move(fn)](EventArgs& a) { fn(a.subject); }); }
        void onTargetChanged(std::function<void(unsigned target, unsigned previousTarget)> fn) { onRaw(Event::TargetChanged, [fn = std::move(fn)](EventArgs& a) { fn(a.subject, static_cast<unsigned>(a.param)); }); }
        void onEntitySpawn(std::function<void(unsigned)> fn) { onRaw(Event::EntitySpawn, [fn = std::move(fn)](EventArgs& a) { fn(a.subject); }); }
        // Payload: the creature address, its category byte, and its health at spawn.
        void onEntitySpawn(std::function<void(unsigned creature, int category, float health)> fn) { onRaw(Event::EntitySpawn, [fn = std::move(fn)](EventArgs& a) { fn(a.subject, a.param, a.amount); }); }
        // Fullest form: also the creature's type/species id (matches CUBE_CATALOG_SPECIES).
        void onEntitySpawn(std::function<void(unsigned creature, int category, int type, float health)> fn) { onRaw(Event::EntitySpawn, [fn = std::move(fn)](EventArgs& a) { fn(a.subject, a.param, a.param2, a.amount); }); }
        void onEntityDeath(std::function<void(unsigned)> fn) { onRaw(Event::EntityDeath, [fn = std::move(fn)](EventArgs& a) { fn(a.subject); }); }
        // Payload: the creature address, its category byte, and (fullest form) its type/species id.
        void onEntityDeath(std::function<void(unsigned creature, int category)> fn) { onRaw(Event::EntityDeath, [fn = std::move(fn)](EventArgs& a) { fn(a.subject, a.param); }); }
        void onEntityDeath(std::function<void(unsigned creature, int category, int type)> fn) { onRaw(Event::EntityDeath, [fn = std::move(fn)](EventArgs& a) { fn(a.subject, a.param, a.param2); }); }
        void onEntityDespawn(std::function<void(unsigned)> fn) { onRaw(Event::EntityDespawn, [fn = std::move(fn)](EventArgs& a) { fn(a.subject); }); }
        // Payload: the last address, its last-known category byte and type/species id.
        void onEntityDespawn(std::function<void(unsigned creature, int category, int type)> fn) { onRaw(Event::EntityDespawn, [fn = std::move(fn)](EventArgs& a) { fn(a.subject, a.param, a.param2); }); }
        void onPetSummoned(std::function<void(unsigned)> fn) { onRaw(Event::PetSummoned, [fn = std::move(fn)](EventArgs& a) { fn(a.subject); }); }
        void onPetDied(std::function<void(unsigned)> fn) { onRaw(Event::PetDied, [fn = std::move(fn)](EventArgs& a) { fn(a.subject); }); }
        void onPetDismissed(std::function<void(unsigned)> fn) { onRaw(Event::PetDismissed, [fn = std::move(fn)](EventArgs& a) { fn(a.subject); }); }
        // Stun / knocked-down edges. Local-player forms are no-arg; entity/pet forms pass the creature address.
        // onStunned also has an overload carrying the stun-lock timer value the loader read for the edge.
        void onStunned(std::function<void()> fn) { on(Event::Stunned, std::move(fn)); }
        void onStunned(std::function<void(int hitStun)> fn) { onRaw(Event::Stunned, [fn = std::move(fn)](EventArgs& a) { fn(a.param); }); }
        void onKnockedDown(std::function<void()> fn) { on(Event::KnockedDown, std::move(fn)); }
        void onRecovered(std::function<void()> fn) { on(Event::Recovered, std::move(fn)); }
        void onEntityStunned(std::function<void(unsigned)> fn) { onRaw(Event::EntityStunned, [fn = std::move(fn)](EventArgs& a) { fn(a.subject); }); }
        void onEntityStunned(std::function<void(unsigned creature, int category, int type)> fn) { onRaw(Event::EntityStunned, [fn = std::move(fn)](EventArgs& a) { fn(a.subject, a.param, a.param2); }); }
        // Complements onEntityStunned: a nearby creature's stun-lock ended.
        void onEntityRecovered(std::function<void(unsigned)> fn) { onRaw(Event::EntityRecovered, [fn = std::move(fn)](EventArgs& a) { fn(a.subject); }); }
        void onEntityRecovered(std::function<void(unsigned creature, int category, int type)> fn) { onRaw(Event::EntityRecovered, [fn = std::move(fn)](EventArgs& a) { fn(a.subject, a.param, a.param2); }); }
        void onEntityKnockedDown(std::function<void(unsigned)> fn) { onRaw(Event::EntityKnockedDown, [fn = std::move(fn)](EventArgs& a) { fn(a.subject); }); }
        void onEntityKnockedDown(std::function<void(unsigned creature, int category, int type)> fn) { onRaw(Event::EntityKnockedDown, [fn = std::move(fn)](EventArgs& a) { fn(a.subject, a.param, a.param2); }); }
        void onPetStunned(std::function<void(unsigned)> fn) { onRaw(Event::PetStunned, [fn = std::move(fn)](EventArgs& a) { fn(a.subject); }); }
        void onPetRecovered(std::function<void(unsigned)> fn) { onRaw(Event::PetRecovered, [fn = std::move(fn)](EventArgs& a) { fn(a.subject); }); }
        void onPetKnockedDown(std::function<void(unsigned)> fn) { onRaw(Event::PetKnockedDown, [fn = std::move(fn)](EventArgs& a) { fn(a.subject); }); }
        // Player selects with R / use key: address = creature/object (0 = world object), kind =
        // SelectionKind, typeByte = the raw target discriminator (container subtype / creature class).
        void onEntitySelected(std::function<void(unsigned, SelectionKind)> fn) { onRaw(Event::EntitySelected, [fn = std::move(fn)](EventArgs& a) { fn(a.subject, static_cast<SelectionKind>(a.param)); }); }
        void onEntitySelected(std::function<void(unsigned target, SelectionKind kind, int typeByte)> fn) { onRaw(Event::EntitySelected, [fn = std::move(fn)](EventArgs& a) { fn(a.subject, static_cast<SelectionKind>(a.param), a.param2); }); }
        // Player picks up an item (E / hold-to-pickup). The full-item overload hands you the picked
        // item (name/material/level/stack); the compact overload passes type, subtype and stack count.
        void onItemPickup(std::function<void()> fn) { on(Event::ItemPickup, std::move(fn)); }
        void onItemPickup(std::function<void(int type, int subtype, int stack)> fn) { onRaw(Event::ItemPickup, [fn = std::move(fn)](EventArgs& a) { fn(static_cast<int>(a.subject), a.param, a.param2); }); }
        void onItemPickup(std::function<void(const Item&)> fn); // defined out-of-line (needs Mod)
        void onCoinsChanged(std::function<void(int)> fn) { onRaw(Event::CoinsChanged, [fn = std::move(fn)](EventArgs& a) { fn(a.param); }); }
        // Payload: the new coin total and the signed delta (positive = gained).
        void onCoinsChanged(std::function<void(int newTotal, int delta)> fn) { onRaw(Event::CoinsChanged, [fn = std::move(fn)](EventArgs& a) { fn(a.param, a.param2); }); }
        void onDayNight(std::function<void(bool)> fn) { onRaw(Event::DayNight, [fn = std::move(fn)](EventArgs& a) { fn(a.param != 0); }); }
        // Payload: whether it is now day, plus the in-game hour [0,23] at the flip.
        void onDayNight(std::function<void(bool isDay, int hour)> fn) { onRaw(Event::DayNight, [fn = std::move(fn)](EventArgs& a) { fn(a.param != 0, a.param2); }); }
        void onBuffGained(std::function<void(int)> fn) { onRaw(Event::BuffGained, [fn = std::move(fn)](EventArgs& a) { fn(a.param); }); }
        // Payload: effect type id, its magnitude, and its remaining duration in ms.
        void onBuffGained(std::function<void(int type, float magnitude, int remainingMs)> fn) { onRaw(Event::BuffGained, [fn = std::move(fn)](EventArgs& a) { fn(a.param, a.amount, a.param2); }); }
        void onBuffLost(std::function<void(int)> fn) { onRaw(Event::BuffLost, [fn = std::move(fn)](EventArgs& a) { fn(a.param); }); }
        void onEquipmentChanged(std::function<void(int)> fn) { onRaw(Event::EquipmentChanged, [fn = std::move(fn)](EventArgs& a) { fn(a.param); }); }
        // Payload: the slot index and the item type now in that slot (0 if it was unequipped).
        void onEquipmentChanged(std::function<void(int slot, int itemType)> fn) { onRaw(Event::EquipmentChanged, [fn = std::move(fn)](EventArgs& a) { fn(a.param, a.param2); }); }
        void onSkillRankChanged(std::function<void(int)> fn) { onRaw(Event::SkillRankChanged, [fn = std::move(fn)](EventArgs& a) { fn(a.param); }); }
        // Payload: the skill index and its new rank.
        void onSkillRankChanged(std::function<void(int index, int newRank)> fn) { onRaw(Event::SkillRankChanged, [fn = std::move(fn)](EventArgs& a) { fn(a.param, a.param2); }); }
        void onAimTargetChanged(std::function<void(unsigned)> fn) { onRaw(Event::AimTargetChanged, [fn = std::move(fn)](EventArgs& a) { fn(a.subject); }); }

    private:
        Mod* m_mod;
    };

    // INTERCEPT. Handlers run ON the game thread when a real game function is called and can cancel it,
    // change its args, or override its return via the HookCall. Keep them tiny; do NOT touch the overlay.
    class EventHook
    {
    public:
        explicit EventHook(Mod* owner) : m_mod(owner) {}

        // Built-in hook (the loader owns the address/convention/marshalling for this CubeHook).
        void on(Hook hook, std::function<void(HookCall&)> fn);
        void onImpact(std::function<void(HookCall&)> fn) { on(Hook::Impact, std::move(fn)); }
        void onCritRoll(std::function<void(HookCall&)> fn) { on(Hook::CritRoll, std::move(fn)); }
        void onMaxHealth(std::function<void(HookCall&)> fn) { on(Hook::MaxHealth, std::move(fn)); }
        // Detach this mod's handlers for a built-in hook (all of them). Returns true if any existed.
        bool remove(Hook hook);

        // Raw hook by an address you found yourself: still yields a HookCall, no detour to write. Give
        // the convention + int-arg count (up to CUBE_HOOK_ARG_MAX int/float args, int return).
        bool raw(unsigned address, CallConv cc, int argCount, std::function<void(HookCall&)> fn);
        // Escape hatch: install your own detour and receive the trampoline (for float/struct returns,
        // odd stdcall arity, etc. that the generic raw path cannot model).
        bool rawDetour(unsigned address, void* detour, void** trampoline);
        // Remove any raw hook (raw or rawDetour) this mod placed at address. Returns true if removed.
        bool removeRaw(unsigned address);
        // Runtime address of a built-in hook's target function (0 if unknown), so a mod can raw-hook or
        // coordinate with it without hardcoding a loader-owned game address.
        unsigned builtinTarget(Hook hook) const;

    private:
        Mod* m_mod;
    };

}
