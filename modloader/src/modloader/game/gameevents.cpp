#include "modloader/game/gameevents.h"
#include "modloader/core/events.h"
#include "hooks/d3d9_hook.h"
#include "core/log.h"
#include "core/mem.h"
#include "game/player.h"
#include "game/gamecontroller.h"
#include "game/attackwatch.h"
#include "game/combat.h"
#include "game/world.h"
#include "game/session.h"
#include "game/status.h"
#include "game/items.h"
#include "game/entities.h"
#include "game/framecache.h"
#include "game/selection.h"
#include "game/pickup.h"
#include "game/offsets.h"
#include "game/diag.h"

#include <algorithm>
#include <cstdint>

namespace modloader::gameevents
{
    namespace
    {

        constexpr char kCategory[] = "gameevents";
        // Live-calibration channel: logs the raw action id whenever it changes so a test run reveals
        // the exact attack/cast ids each weapon produces (the id partition is weapon/class dependent).
        constexpr char kActionCategory[] = "action";

        uint32_t g_frameIndex = 0; // render thread only

        // Snapshot of last frame's watched values, diffed each frame to detect edges. Render-thread only.
        struct PrevGameState
        {
            bool valid = false;
            bool hadArea = false;
            int32_t attacking = 0;
            int32_t lastActionId = -1;
            int32_t onGround = 1;
            int32_t alive = 1;
            int32_t movement = 0;
            uint32_t target = 0;
            int32_t coins = 0;
            int32_t stunned = 0;
            int32_t knockedDown = 0;
            uint64_t petId = 0;
            uint32_t petAddress = 0;
            int32_t zoneX = 0;
            int32_t zoneY = 0;
            int32_t level = 0;
            bool menuOpen = false;
            bool hadTime = false;
            int32_t isDay = 0;
            uint64_t aimId = 0;
            bool hadBuffs = false;
            int32_t buffCount = 0;
            uint8_t buffTypes[CUBE_BUFFS_MAX] = {};
            bool hadEquip = false;
            uint32_t equipSig[CUBE_EQUIP_SLOTS] = {};
            bool hadSkills = false;
            int32_t skills[CUBE_SKILL_COUNT] = {};
            bool hadCooldowns = false;
            int32_t cdCount = 0;
            int32_t cdIds[CUBE_ABILITIES_MAX] = {};
            int32_t cdMs[CUBE_ABILITIES_MAX] = {};
        };

        PrevGameState g_prev; // render thread only

        // Build a CubeEventArgs carrying the event's real payload (subject/param/param2/amount) and emit it.
        void emitEvent(CubeEvent event, uint32_t subject, int32_t param, int32_t param2 = 0, float amount = 0.0f)
        {
            CubeEventArgs args = {};
            args.structSize = sizeof(CubeEventArgs);
            args.event = event;
            args.subject = subject;
            args.param = param;
            args.param2 = param2;
            args.amount = amount;
            // One detailed line per emitted event, with the FULL raw payload every mod receives
            // (subject address + the three data fields). Replaces the per-event scalar lines so every
            // event - not just a hand-picked few - is logged identically. See the SDK event catalog
            // for what param/param2/amount mean per event.
            LOGC(Debug, kCategory, "emit %s: subject=0x%08X param=%d param2=%d amount=%.2f",
                 modloader::events::eventName(event), subject, param, param2, amount);
            modloader::events::emit(args);
        }

        constexpr uint32_t kSigMultiplier = 131u; // small odd prime for the mixing hash
        constexpr uint32_t kSigEmpty = 0u; // signature of an absent item (empty slot)
        constexpr uint32_t kSigNonEmpty = 1u; // fallback so a present item never reads as empty

        // A stable per-item signature to detect an equipment slot change without storing the item.
        // Change detection only, not identity.
        uint32_t itemSignature(const CubeItem& item)
        {
            if (!item.present)
                return kSigEmpty;

            uint32_t sig = static_cast<uint32_t>(item.type);
            sig = sig * kSigMultiplier + static_cast<uint32_t>(item.subtype);
            sig = sig * kSigMultiplier + static_cast<uint32_t>(item.material);
            sig = sig * kSigMultiplier + static_cast<uint32_t>(item.level);
            sig = sig * kSigMultiplier + static_cast<uint32_t>(item.upgradeCount);
            sig ^= item.seed;

            return sig ? sig : kSigNonEmpty;
        }

        // Emit a BUFF_GAINED carrying the effect's magnitude + remaining duration, looked up from the
        // live buff list by type (the first match; duplicate types share one lookup - documented in the SDK).
        void emitBuffGained(uint8_t type, const CubeBuff* live, int32_t liveN)
        {
            for (int32_t i = 0; i < liveN; ++i)
            {
                if (static_cast<uint8_t>(live[i].type) == type)
                {
                    emitEvent(CUBE_EVENT_BUFF_GAINED, 0, type, live[i].remainingMs, live[i].magnitude);
                    return;
                }
            }
            emitEvent(CUBE_EVENT_BUFF_GAINED, 0, type); // not found (raced out already): type only
        }

        // Emit BUFF_GAINED / BUFF_LOST for the multiset difference of two sorted type lists. `live`/`liveN`
        // is the current unsorted buff list, so a gained buff carries its magnitude + duration.
        void diffBuffTypes(const uint8_t* cur, int32_t curN, const uint8_t* prev, int32_t prevN,
            const CubeBuff* live, int32_t liveN)
        {
            int32_t i = 0;
            int32_t j = 0;
            while (i < curN && j < prevN)
            {
                if (cur[i] == prev[j])
                {
                    ++i;
                    ++j;
                }
                else if (cur[i] < prev[j])
                {
                    emitBuffGained(cur[i], live, liveN);
                    ++i;
                }
                else
                {
                    emitEvent(CUBE_EVENT_BUFF_LOST, 0, prev[j]);
                    ++j;
                }
            }
            for (; i < curN; ++i)
                emitBuffGained(cur[i], live, liveN);
            for (; j < prevN; ++j)
                emitEvent(CUBE_EVENT_BUFF_LOST, 0, prev[j]);
        }

        // Diff the player's buffs, equipment and skill ranks against last frame, emitting
        // BUFF_*/EQUIPMENT_CHANGED/SKILL_RANK_CHANGED.
        void pollInventoryState()
        {
            CubeBuff buffs[CUBE_BUFFS_MAX];
            const int32_t buffCount = game::listBuffs(buffs, CUBE_BUFFS_MAX);
            uint8_t types[CUBE_BUFFS_MAX];

            for (int32_t i = 0; i < buffCount; ++i)
                types[i] = static_cast<uint8_t>(buffs[i].type);
            std::sort(types, types + buffCount);

            if (g_prev.hadBuffs)
                diffBuffTypes(types, buffCount, g_prev.buffTypes, g_prev.buffCount, buffs, buffCount);
            std::copy(types, types + buffCount, g_prev.buffTypes);

            g_prev.buffCount = buffCount;
            g_prev.hadBuffs = true;

            CubeItem equip[CUBE_EQUIP_SLOTS];
            const int32_t equipCount = game::listEquipment(equip, CUBE_EQUIP_SLOTS);
            uint32_t sig[CUBE_EQUIP_SLOTS] = {};
            int32_t slotType[CUBE_EQUIP_SLOTS] = {}; // the item type now in each slot (0 = empty)

            for (int32_t i = 0; i < equipCount; ++i)
            {
                const int32_t slot = equip[i].slot;
                if (slot >= 0 && slot < CUBE_EQUIP_SLOTS)
                {
                    sig[slot] = itemSignature(equip[i]);
                    slotType[slot] = equip[i].present ? equip[i].type : 0;
                }
            }

            if (g_prev.hadEquip)
            {
                for (int32_t slot = 0; slot < CUBE_EQUIP_SLOTS; ++slot)
                {
                    // param = slot, param2 = the item type now in that slot (0 = unequipped)
                    if (sig[slot] != g_prev.equipSig[slot])
                        emitEvent(CUBE_EVENT_EQUIPMENT_CHANGED, 0, slot, slotType[slot]);
                }
            }

            std::copy(sig, sig + CUBE_EQUIP_SLOTS, g_prev.equipSig);
            g_prev.hadEquip = true;

            int32_t ranks[CUBE_SKILL_COUNT];
            const int32_t skillCount = game::listSkillRanks(ranks, CUBE_SKILL_COUNT);

            if (g_prev.hadSkills)
            {
                for (int32_t i = 0; i < skillCount; ++i)
                {
                    if (ranks[i] != g_prev.skills[i])
                        emitEvent(CUBE_EVENT_SKILL_RANK_CHANGED, 0, i, ranks[i]);
                }
            }

            std::copy(ranks, ranks + skillCount, g_prev.skills);
            g_prev.hadSkills = true;
        }

        // Diff the aim/hover target id; on change emit AIM_TARGET_CHANGED with the resolved creature
        // address (0 if none). The tree walk to resolve the id runs only on a change edge.
        void pollAimTarget()
        {
            uintptr_t gc = 0;

            if (!game::resolveGameController(gc))
            {
                g_prev.aimId = 0;
                return;
            }

            uint64_t aimId = 0;

            if (!mem::read(gc + off::kAimTargetIdOff, aimId) || aimId == g_prev.aimId)
                return;
            uint32_t subject = 0;
            uintptr_t creature = 0;
            if (aimId && game::findCreatureById(gc, aimId, creature))
                subject = static_cast<uint32_t>(creature);
            emitEvent(CUBE_EVENT_AIM_TARGET_CHANGED, subject, 0);
            g_prev.aimId = aimId;
        }

        // Diff the pet id for PET_SUMMONED/PET_DISMISSED. Pet address is cached on the summon edge so
        // PET_DIED matches the per-frame ENTITY_DEATH edges without an extra tree walk.
        void pollPetLifecycle(const CubePlayer& player, bool okPlayer)
        {
            if (!okPlayer || !player.address)
            {
                g_prev.petId = 0;
                g_prev.petAddress = 0;
                return;
            }
            uint64_t petId = 0;
            // A failed read must not zero petId and falsely fire PET_DISMISSED; leave prev state intact.
            if (!mem::read(static_cast<uintptr_t>(player.address) + off::kPetIdOff, petId))
                return;
            if (petId == g_prev.petId)
                return;

            if (g_prev.petId != 0)
            {
                emitEvent(CUBE_EVENT_PET_DISMISSED, g_prev.petAddress, 0);
            }
            uint32_t petAddress = 0;
            if (petId != 0)
            {
                uintptr_t gc = 0;
                uintptr_t creature = 0;
                if (game::resolveGameController(gc) && game::findCreatureById(gc, petId, creature))
                    petAddress = static_cast<uint32_t>(creature);
                emitEvent(CUBE_EVENT_PET_SUMMONED, petAddress, 0);
            }
            g_prev.petAddress = petAddress;
            g_prev.petId = petId;
        }

        int32_t prevCooldownMs(int32_t abilityId)
        {
            for (int32_t i = 0; i < g_prev.cdCount; ++i)
            {
                if (g_prev.cdIds[i] == abilityId)
                    return g_prev.cdMs[i];
            }
            return 0;
        }

        // Diff the ability-cooldown map: an ability whose remaining-ms goes from ready (absent/0) to a
        // fresh positive value was just cast. The map key is the ability id (matches CUBE_CATALOG_ABILITY).
        void pollAbilityUse(uint32_t playerAddress)
        {
            CubeAbilityCooldown cooldowns[CUBE_ABILITIES_MAX];
            const int32_t count = game::listAbilityCooldowns(cooldowns, CUBE_ABILITIES_MAX);

            if (g_prev.hadCooldowns)
            {
                for (int32_t i = 0; i < count; ++i)
                {
                    if (cooldowns[i].remainingMs > 0 && prevCooldownMs(cooldowns[i].abilityId) <= 0)
                    {
                        emitEvent(CUBE_EVENT_ABILITY_USED, playerAddress, cooldowns[i].abilityId, cooldowns[i].remainingMs);
                    }
                }
            }

            g_prev.cdCount = count < CUBE_ABILITIES_MAX ? count : CUBE_ABILITIES_MAX;
            for (int32_t i = 0; i < g_prev.cdCount; ++i)
            {
                g_prev.cdIds[i] = cooldowns[i].abilityId;
                g_prev.cdMs[i] = cooldowns[i].remainingMs;
            }
            g_prev.hadCooldowns = true;
        }

        // Poll local player/world each frame and emit on edges (attack, jump, zone change).
        // Fires nothing until the relevant offsets resolve (hasState/hasArea).
        void pollGameEvents()
        {
            CubePlayer player = {};
            player.structSize = sizeof(CubePlayer);
            const bool okPlayer = game::readPlayer(player);

            CubeWorld world = {};
            world.structSize = sizeof(CubeWorld);
            const bool okWorld = game::readWorld(world);

            if (okPlayer && player.hasState)
            {
                // Tell the game-thread attack watcher who the local player is (cheap per-tick filter).
                game::attackwatch::setLocalPlayer(player.address);

                // Stun-lock timer and the downed action drive the stun edges.
                int32_t hitStun = 0;
                // A failed read leaves hitStun 0; do not treat that as "recovered" or lose the prior state.
                const bool stunReadOk = mem::read(static_cast<uintptr_t>(player.address) + off::kPlayerHitStunOff, hitStun);
                const int32_t stunnedNow = (hitStun > 0) ? 1 : 0;
                if (g_prev.valid)
                {
                    // PLAYER_ATTACK is detected on the game thread by the attack watcher (it catches a
                    // sub-frame shot/ability action pulse the frame poll misses). Only fall back to the
                    // frame-poll edge if the watcher could not arm its detour.
                    if (!game::attackwatch::active() && !g_prev.attacking && player.attacking)
                    {
                        // param = action id, param2 = the selected target at the edge (0 if none)
                        emitEvent(CUBE_EVENT_PLAYER_ATTACK, player.address, player.actionId, static_cast<int32_t>(player.target));
                    }
                    if (g_prev.onGround && !player.onGround)
                    {
                        emitEvent(CUBE_EVENT_PLAYER_JUMP, player.address, 0, 0, player.velZ);
                    }
                    if (!g_prev.onGround && player.onGround)
                    {
                        emitEvent(CUBE_EVENT_PLAYER_LAND, player.address, 0, 0, player.velZ);
                    }
                    if (player.level > g_prev.level)
                    {
                        emitEvent(CUBE_EVENT_PLAYER_LEVELUP, player.address, player.level, g_prev.level);
                    }
                    if (g_prev.alive && !player.alive)
                    {
                        emitEvent(CUBE_EVENT_PLAYER_DEATH, player.address, 0);
                    }
                    if (!g_prev.alive && player.alive)
                    {
                        emitEvent(CUBE_EVENT_PLAYER_RESPAWN, player.address, 0);
                    }
                    if (player.movement != g_prev.movement)
                    {
                        emitEvent(CUBE_EVENT_MOVEMENT_CHANGED, player.address, player.movement, g_prev.movement);
                    }
                    if (player.target != g_prev.target)
                    {
                        emitEvent(CUBE_EVENT_TARGET_CHANGED, player.target, static_cast<int32_t>(g_prev.target));
                    }
                    if (player.coins != g_prev.coins)
                    {
                        emitEvent(CUBE_EVENT_COINS_CHANGED, 0, player.coins, player.coins - g_prev.coins);
                    }
                    if (stunReadOk && !g_prev.stunned && stunnedNow)
                    {
                        emitEvent(CUBE_EVENT_PLAYER_STUNNED, player.address, hitStun);
                    }
                    if (stunReadOk && g_prev.stunned && !stunnedNow)
                    {
                        emitEvent(CUBE_EVENT_PLAYER_RECOVERED, player.address, 0);
                    }
                    if (!g_prev.knockedDown && player.knockedDown)
                    {
                        emitEvent(CUBE_EVENT_PLAYER_KNOCKED_DOWN, player.address, 0);
                    }
                }

                // Game-thread attack watcher: emit PLAYER_ATTACK for the attack/shot/ability edge(s) the
                // behavior detour captured since the last frame - reliable for the sub-frame action pulses
                // the poll above cannot see. param = action id at the edge, param2 = current selected target.
                if (game::attackwatch::active())
                {
                    int32_t attackAction = 0;
                    uint32_t attackCount = 0;
                    if (game::attackwatch::pollAttack(attackAction, attackCount))
                    {
                        emitEvent(CUBE_EVENT_PLAYER_ATTACK, player.address, attackAction, static_cast<int32_t>(player.target));
                    }
                }

                // Calibration trace: log every action-id change (with the loader's attack/movement
                // classification) so a live test run confirms which ids each weapon actually uses.
                if (player.actionId != g_prev.lastActionId)
                {
                    LOGC(Debug, kActionCategory, "action %d -> %d (attacking=%d action=%d elapsed=%dms)",
                        g_prev.lastActionId, player.actionId, player.attacking, player.action, player.actionElapsedMs);
                    g_prev.lastActionId = player.actionId;
                }

                g_prev.attacking = player.attacking;
                g_prev.onGround = player.onGround;
                g_prev.level = player.level;
                g_prev.alive = player.alive;
                g_prev.movement = player.movement;
                g_prev.target = player.target;
                g_prev.coins = player.coins;
                if (stunReadOk)
                    g_prev.stunned = stunnedNow;
                g_prev.knockedDown = player.knockedDown;
                g_prev.valid = true;
                pollInventoryState();
                pollAimTarget();
                pollAbilityUse(player.address);
            }
            else
            {
                // Player gone (title/menu); do not diff across the gap.
                game::attackwatch::setLocalPlayer(0);
                g_prev.valid = false;
                g_prev.hadBuffs = false;
                g_prev.hadEquip = false;
                g_prev.hadSkills = false;
                g_prev.hadCooldowns = false;
                g_prev.aimId = 0;
            }

            // A single frame can spawn and despawn a full zone (mass reload) and also damage every
            // creature (mass AoE), so the edge buffer holds several edges per creature. Overflow is
            // guarded (extra edges are dropped, never a crash).
            constexpr int32_t kMaxEntityEdges = CUBE_ENTITIES_MAX * 3;
            game::EntityEdge entityEdges[kMaxEntityEdges];
            int32_t entityEdgeCount = 0;
            const game::CombatEdges combat = game::pollCombat(player, okPlayer, entityEdges, kMaxEntityEdges, entityEdgeCount);
            if (combat.damageTaken > 0.0f)
            {
                const int32_t damage = static_cast<int32_t>(combat.damageTaken);
                emitEvent(CUBE_EVENT_PLAYER_DAMAGED, player.address, damage,
                    static_cast<int32_t>(player.health), combat.damageTaken);
            }
            if (combat.crit)
            {
                emitEvent(CUBE_EVENT_PLAYER_CRIT, 0, 0);
            }
            for (int32_t i = 0; i < entityEdgeCount; ++i)
            {
                const game::EntityEdge& edge = entityEdges[i];
                const bool isPet = g_prev.petAddress != 0 && edge.address == g_prev.petAddress;
                switch (edge.kind)
                {
                    case game::EntityEdgeKind::Damaged:
                        // ANY creature took damage (attacker not attributed). subject = victim,
                        // param = victim category, param2 = remaining health, amount = damage.
                        emitEvent(CUBE_EVENT_ENTITY_DAMAGED, edge.address, edge.category,
                            static_cast<int32_t>(edge.health), edge.damage);
                        break;
                    case game::EntityEdgeKind::Spawn:
                        emitEvent(CUBE_EVENT_ENTITY_SPAWN, edge.address, edge.category, edge.type, edge.health);
                        break;
                    case game::EntityEdgeKind::Death:
                        emitEvent(CUBE_EVENT_ENTITY_DEATH, edge.address, edge.category, edge.type);
                        if (isPet)
                            emitEvent(CUBE_EVENT_PET_DIED, edge.address, edge.category, edge.type);
                        break;
                    case game::EntityEdgeKind::Despawn:
                        emitEvent(CUBE_EVENT_ENTITY_DESPAWN, edge.address, edge.category, edge.type);
                        break;
                    case game::EntityEdgeKind::Stunned:
                        emitEvent(CUBE_EVENT_ENTITY_STUNNED, edge.address, edge.category, edge.type);
                        if (isPet)
                            emitEvent(CUBE_EVENT_PET_STUNNED, edge.address, edge.category, edge.type);
                        break;
                    case game::EntityEdgeKind::Recovered:
                        emitEvent(CUBE_EVENT_ENTITY_RECOVERED, edge.address, edge.category, edge.type);
                        if (isPet)
                            emitEvent(CUBE_EVENT_PET_RECOVERED, edge.address, edge.category, edge.type);
                        break;
                    case game::EntityEdgeKind::KnockedDown:
                        emitEvent(CUBE_EVENT_ENTITY_KNOCKED_DOWN, edge.address, edge.category, edge.type);
                        if (isPet)
                            emitEvent(CUBE_EVENT_PET_KNOCKED_DOWN, edge.address, edge.category, edge.type);
                        break;
                }
            }
            pollPetLifecycle(player, okPlayer);

            CubeUi ui = {};
            ui.structSize = sizeof(CubeUi);
            if (game::readUi(ui))
            {
                const bool open = ui.anyOpen != 0;
                if (open != g_prev.menuOpen)
                {
                    // param = a bitmask of which tracked HUD panels are open (see CubeUiMenuMask).
                    const int32_t mask = (ui.inventoryOpen ? CUBE_MENU_INVENTORY : 0)
                        | (ui.characterOpen ? CUBE_MENU_CHARACTER : 0)
                        | (ui.mapOpen ? CUBE_MENU_MAP : 0)
                        | (ui.objectiveOpen ? CUBE_MENU_OBJECTIVE : 0);
                    emitEvent(open ? CUBE_EVENT_MENU_OPEN : CUBE_EVENT_MENU_CLOSE, 0, mask);
                    g_prev.menuOpen = open;
                }
            }

            if (okWorld && world.hasArea)
            {
                if (g_prev.hadArea && (g_prev.zoneX != world.zoneX || g_prev.zoneY != world.zoneY))
                {
                    emitEvent(CUBE_EVENT_AREA_CHANGE, 0, world.zoneX, world.zoneY);
                }
                g_prev.zoneX = world.zoneX;
                g_prev.zoneY = world.zoneY;
                g_prev.hadArea = true;
            }
            else
            {
                g_prev.hadArea = false;
            }

            if (okWorld && world.hasTime)
            {
                if (g_prev.hadTime && world.isDay != g_prev.isDay)
                {
                    emitEvent(CUBE_EVENT_DAY_NIGHT, 0, world.isDay, world.hour);
                }
                g_prev.isDay = world.isDay;
                g_prev.hadTime = true;
            }
            else
                g_prev.hadTime = false;

            // Select detour captured a new R/use-key selection: emit it on the render thread
            // (subject = creature/object, param = CubeSelectionKind).
            CubeSelection selection = {};
            selection.structSize = sizeof(CubeSelection);
            if (game::selection::pollSelection(selection))
            {
                emitEvent(CUBE_EVENT_ENTITY_SELECTED, selection.address, selection.kind, selection.typeByte);
            }

            // Pickup detour captured a new E-key item pickup: emit it on the render thread
            // (subject = item type, param = subtype, param2 = stack count).
            CubeItem pickup = {};
            pickup.structSize = sizeof(CubeItem);
            if (game::pickup::pollPickup(pickup))
            {
                emitEvent(CUBE_EVENT_ITEM_PICKUP, static_cast<uint32_t>(pickup.type), pickup.subtype, pickup.stack);
            }

            // Emits the full offset-resolution report the first frame a world loads.
            game::diag::pollResolutionReport();
            // Warns if any held/equipped/stored item is corrupt (throttled + de-duplicated).
            game::diag::pollItemCorruption();
        }
    }

    void emitLifecycle(CubeEvent event)
    {
        emitEvent(event, 0, 0);
    }

    uint32_t currentFrame()
    {
        return g_frameIndex;
    }

    void CUBE_CALL onFrame(IDirect3DDevice9* device)
    {
        // Open the intra-frame resolve cache so the many player/GC/entity resolves below walk the
        // pointer chains only once.
        game::framecache::beginFrame();

        CubeEventArgs args = {};
        args.structSize = sizeof(CubeEventArgs);
        args.event = CUBE_EVENT_FRAME;
        args.device = device;
        args.hwnd = hooks::d3d9::window();
        args.frameIndex = g_frameIndex++;
        modloader::events::emit(args);

        pollGameEvents();
    }

    void CUBE_CALL onDeviceReset(bool preReset)
    {
        CubeEventArgs args = {};
        args.structSize = sizeof(CubeEventArgs);
        args.event = CUBE_EVENT_DEVICE_RESET;
        args.preReset = preReset ? 1 : 0;
        modloader::events::emit(args);
    }

    bool CUBE_CALL onWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        CubeEventArgs args = {};
        args.structSize = sizeof(CubeEventArgs);
        args.event = CUBE_EVENT_WNDPROC;
        args.hwnd = hwnd;
        args.msg = msg;
        args.wParam = static_cast<uint32_t>(wParam);
        args.lParam = static_cast<int32_t>(lParam);
        return modloader::events::emit(args) != 0;
    }
}