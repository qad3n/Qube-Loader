#include "game/diag.h"
#include "game/offsets.h"
#include "game/catalog.h"
#include "game/player.h"
#include "game/gamecontroller.h"
#include "game/combat.h"
#include "game/items.h"
#include "game/status.h"
#include "game/world.h"
#include "game/pet.h"
#include "game/entities.h"
#include "game/view.h"
#include "game/session.h"
#include "core/log.h"
#include "core/mem.h"
#include "util/fmt.h"

#include <cstdint>

namespace game::diag
{
    namespace
    {
        constexpr char kCategory[] = "diag";
        constexpr int32_t kMaxEquipRowsLogged = 4;
        constexpr int32_t kMaxEntityRowsLogged = 5;

        const char* movementName(int32_t movement)
        {
            switch (movement)
            {
                case CUBE_MOVE_GROUNDED:
                    return "grounded";
                case CUBE_MOVE_AIRBORNE:
                    return "airborne";
                case CUBE_MOVE_CLIMBING:
                    return "climbing";
                case CUBE_MOVE_SWIMMING:
                    return "swimming";
                case CUBE_MOVE_GLIDING:
                    return "gliding";
                case CUBE_MOVE_SNEAKING:
                    return "sneaking";
                case CUBE_MOVE_SITTING:
                    return "sitting";
                default:
                    return "unknown";
            }
        }

        const char* actionName(int32_t action)
        {
            switch (action)
            {
                case CUBE_ACTION_IDLE:
                    return "idle";
                case CUBE_ACTION_ATTACKING:
                    return "attacking";
                case CUBE_ACTION_CASTING:
                    return "casting";
                case CUBE_ACTION_BLOCKING:
                    return "blocking";
                case CUBE_ACTION_ROLLING:
                    return "rolling";
                case CUBE_ACTION_DEAD:
                    return "dead";
                default:
                    return "unknown";
            }
        }

        void logPlayer()
        {
            CubePlayer p = {};
            p.structSize = sizeof(p);
            if (!readPlayer(p))
            {
                LOGC(Debug, kCategory, "player: UNAVAILABLE (no character in world)");
                return;
            }

            LOGC(Debug, kCategory, "player: RESOLVED @0x%08X (vtable verified; all pinned fields readable)", p.address);
            LOGC(Debug, kCategory, "  id: name='%s'%s class=%s(%d) spec=%d type=%d level=%d xp=%u", p.name, p.hasName ? "" : " (unresolved)", p.className, p.classForm, p.spec, p.type, p.level, p.xp);
            LOGC(Debug, kCategory, "  vitals: hp=%.1f%s mana=%.0f%% stamina=%.0f%% coins=%d", p.health, p.alive ? "" : " (dead)", p.manaPercent, p.staminaPercent, p.coins);
            LOGC(Debug, kCategory, "  state: movement=%s action=%s facing=%.2f speed=%.1f sneaking=%d", movementName(p.movement), actionName(p.action), p.facing, p.speed, p.sneaking);
            LOGC(Debug, kCategory, "  pos: %.1f, %.1f, %.1f  vel: %.2f, %.2f, %.2f", p.x, p.y, p.z, p.velX, p.velY, p.velZ);

            // Spawn-field disambiguation: the candidate whose coord>>6 == region and coord ==
            // zone is the bound-home field.
            if (p.address)
            {
                int32_t aX = 0;
                int32_t aY = 0;
                int32_t bX = 0;
                int32_t bY = 0;
                uint8_t flag = 0;
                mem::read(p.address + off::kSpawnZoneXAltOff, aX);
                mem::read(p.address + off::kSpawnZoneYAltOff, aY);
                mem::read(p.address + off::kSpawnFlagOff, flag);
                mem::read(p.address + off::kSpawnZoneXOff, bX);
                mem::read(p.address + off::kSpawnZoneYOff, bY);
                LOGC(Debug, kCategory, "  spawn-probe: A(+1b0)=%d,%d region>>6=%d,%d | flag(+1d8)=%d | B(+1dc)=%d,%d region>>6=%d,%d",
                     aX, aY, aX >> off::kZoneToRegionShift, aY >> off::kZoneToRegionShift,
                     flag, bX, bY, bX >> off::kZoneToRegionShift, bY >> off::kZoneToRegionShift);
            }

            CubeCombat c = {};
            c.structSize = sizeof(c);

            if (readCombat(c))
                LOGC(Debug, kCategory, "  combat: power=%.1f armor=%.1f spirit=%.1f combo=%d atkSpeed=%.2f cooldown=%.2f crit~%.0f%% hitStun=%d", c.power, c.armor, c.spirit, c.combo, c.attackSpeed, c.attackCooldown, c.critChancePercent, c.hitStun);

            CubeItem equipment[CUBE_EQUIP_SLOTS];
            CubeItem inventory[CUBE_INVENTORY_MAX];
            int32_t ranks[CUBE_SKILL_COUNT];
            CubeBuff buffs[CUBE_BUFFS_MAX];

            const int32_t equipCount = listEquipment(equipment, CUBE_EQUIP_SLOTS);
            const int32_t bagCount = listInventory(inventory, CUBE_INVENTORY_MAX);
            const int32_t skillCount = listSkillRanks(ranks, CUBE_SKILL_COUNT);
            const int32_t buffCount = listBuffs(buffs, CUBE_BUFFS_MAX);

            LOGC(Debug, kCategory, "  items: equipped=%d bag=%d skills=%d buffs=%d", equipCount, bagCount, skillCount, buffCount);

            for (int32_t i = 0; i < equipCount && i < kMaxEquipRowsLogged; ++i)
            {
                const CubeItem& it = equipment[i];
                const char* typeName = catalogName(CUBE_CATALOG_ITEM_TYPE, it.type);
                const char* subName = catalogName(CUBE_CATALOG_WEAPON_SUBTYPE, it.subtype);
                const char* matName = catalogName(CUBE_CATALOG_MATERIAL, it.material);
                LOGC(Debug, kCategory, "equip slot%d: %s %s (type=%d sub=%d mat=%d lvl=%d)", it.slot, matName ? matName : "?", subName ? subName : (typeName ? typeName : "?"), it.type, it.subtype, it.material, it.level);
            }
        }

        void logWorld()
        {
            CubeWorld w = {};
            w.structSize = sizeof(w);
            if (!readWorld(w))
            {
                LOGC(Debug, kCategory, "world: UNAVAILABLE");
                return;
            }

            LOGC(Debug, kCategory, "world: RESOLVED @0x%08X zone %d,%d region %d,%d", w.address, w.zoneX, w.zoneY, w.regionX, w.regionY);

            if (w.hasClimate)
                LOGC(Debug, kCategory, "  climate: temp=%.2f humidity=%.2f elevation=%.1f terrain=%d", w.temperature, w.humidity, w.elevation, w.terrainType);
            else
                LOGC(Debug, kCategory, "  climate: unavailable (player tile not resident)");
            if (w.hasTime)
                LOGC(Debug, kCategory, "  time: %02d:%02d (%s)", w.hour, w.minute, w.isDay ? "day" : "night");
            if (w.hasSpawn)
                LOGC(Debug, kCategory, "  spawn: %.0f, %.0f, %.0f", w.spawnX, w.spawnY, w.spawnZ);

            LOGC(Debug, kCategory, "  seed: %s", w.hasSeed ? "" : "unavailable");

            if (w.hasSeed)
                LOGC(Debug, kCategory, "  seed = 0x%08X", w.seed);

            CubeStructure structures[CUBE_STRUCTURES_MAX];
            const int32_t structureCount = listStructures(structures, CUBE_STRUCTURES_MAX);

            LOGC(Debug, kCategory, "  structures: %d in zone", structureCount);
        }

        void logEntities()
        {
            CubeEntity entities[CUBE_ENTITIES_MAX];
            const int32_t count = listEntities(entities, CUBE_ENTITIES_MAX);
            LOGC(Debug, kCategory, "entities: %d nearby", count);

            for (int32_t i = 0; i < count && i < kMaxEntityRowsLogged; ++i)
                LOGC(Debug, kCategory, "  [%d] '%s' type=%d lvl=%d hp=%.0f relation=%d",
                     i, entities[i].name, entities[i].type, entities[i].level, entities[i].health, entities[i].relation);

            CubeEntity target = {};
            target.structSize = sizeof(target);

            if (targetEntity(target))
                LOGC(Debug, kCategory, "  target: '%s' lvl %d hp %.0f relation=%d", target.hasName ? target.name : "?", target.level, target.health, target.relation);

            CubePet pet = {};
            pet.structSize = sizeof(pet);

            if (readPet(pet))
                LOGC(Debug, kCategory, "  pet: '%s' type %d level %d hp %.0f", pet.hasName ? pet.name : "?", pet.type, pet.level, pet.health);
        }

        void logView()
        {
            CubeCamera cam = {};
            cam.structSize = sizeof(cam);
            if (readCamera(cam))
                LOGC(Debug, kCategory, "camera: dist=%.1f pitch=%.1f yaw=%.1f fov=%.2f firstPerson=%d matrices=%s",
                     cam.distance, cam.pitch, cam.yaw, cam.fov, cam.firstPerson, cam.hasMatrices ? "yes" : "no");
            else
                LOGC(Debug, kCategory, "camera: UNAVAILABLE");

            CubeDisplay disp = {};
            disp.structSize = sizeof(disp);

            if (readDisplay(disp))
            {
                LOGC(Debug, kCategory, "display: %dx%d %s", disp.width, disp.height,
                     disp.fullscreen ? "fullscreen" : "windowed");

                if (disp.hasSettings)
                    LOGC(Debug, kCategory, "  settings: renderDist=%d res=%dx%d sound=%d music=%d fpsCap(minStep)=%d", disp.renderDistance, disp.resolutionX, disp.resolutionY, disp.soundVolume, disp.musicVolume, disp.minTimeStep);
            }
            else
                LOGC(Debug, kCategory, "display: UNAVAILABLE");
        }

        void logSession()
        {
            CubeSession s = {};
            s.structSize = sizeof(s);

            if (readSession(s))
                LOGC(Debug, kCategory, "session: %s network=%s connected=%d players=%d", s.gameState == CUBE_STATE_IN_WORLD ? "in-world" : "title", s.hasNetwork ? (s.networkMode == CUBE_NET_MULTIPLAYER ? "multiplayer" : "singleplayer") : "?", s.connected, s.playerCount);
            else
                LOGC(Debug, kCategory, "session: UNAVAILABLE");

            CubeUi ui = {};
            ui.structSize = sizeof(ui);

            if (readUi(ui))
                LOGC(Debug, kCategory, "ui: menu=%s (inventory=%d character=%d map=%d objective=%d)", ui.anyOpen ? "OPEN" : "closed", ui.inventoryOpen, ui.characterOpen, ui.mapOpen, ui.objectiveOpen);
        }

    }

    void logResolutionReport()
    {
        LOGC(Debug, kCategory, "=== resolution report ===");
        uintptr_t gc = 0;

        if (!resolveGameController(gc))
        {
            LOGC(Debug, kCategory, "GameController: UNAVAILABLE (game not fully initialized; report will retry live)");
            LOGC(Debug, kCategory, "=== end report ===");
            return;
        }

        LOGC(Debug, kCategory, "GameController: RESOLVED @0x%08X", fmt::u32(gc));

        logSession();
        logPlayer();
        logWorld();
        logEntities();
        logView();

        LOGC(Debug, kCategory, "=== end report ===");
    }

    void logKnownGaps()
    {
        LOGC(Debug, kCategory, "known deferred fields (intentionally unavailable, NOT resolution failures):");
        LOGC(Debug, kCategory, "  entity maxHealth = computed (FUN_00444db0); weather/season = absent in build;");
        LOGC(Debug, kCategory, "  pet mood = absent; race/gender + quests = offset +0x1d28 contested (gated);");
        LOGC(Debug, kCategory, "  region name = needs game-call; crafting = recipe-walk pending.");
    }

    void pollResolutionReport()
    {
        static bool g_reported = false; // render thread only
        uintptr_t gc = 0;
        uintptr_t creature = 0;
        const bool inWorld = resolveLocalPlayer(gc, creature);

        if (inWorld && !g_reported)
        {
            LOGC(Debug, kCategory, "world/player resolved live - full offset report follows");
            logResolutionReport();
            g_reported = true;
        }
        else if (!inWorld && g_reported)
        {
            LOGC(Debug, kCategory, "player/world no longer resolved (left world / title screen)");
            g_reported = false;
        }
    }

    namespace
    {

        constexpr char kItemCategory[] = "itemscan";
        constexpr int32_t kMaxItemDefects = 16; // reported per scan
        constexpr int32_t kMaxWarnedDefects = 64; // distinct signatures remembered (anti-spam)
        constexpr int32_t kItemScanIntervalFrames = 180; // rescan cadence while in-world
        constexpr uint32_t kDefectTypeShift = 8;
        constexpr uint32_t kDefectSubtypeShift = 16;

        // Defect identity for de-dup: address + type/subtype, so a slot mutating to a new
        // bad value re-warns.
        uint32_t defectSignature(const ItemDefect& defect)
        {
            return defect.address ^ (static_cast<uint32_t>(defect.type) << kDefectTypeShift) ^
                   (static_cast<uint32_t>(defect.subtype) << kDefectSubtypeShift);
        }

        const char* defectLocationName(ItemDefectLocation location)
        {
            switch (location)
            {
                case ItemDefectLocation::Held:
                    return "HELD/cursor";
                case ItemDefectLocation::Equipment:
                    return "equipment";
                case ItemDefectLocation::Inventory:
                    return "inventory";
                default:
                    return "item";
            }
        }

        bool containsSignature(uint32_t signature, const uint32_t* set, int32_t count)
        {
            for (int32_t i = 0; i < count; ++i)
            {
                if (set[i] == signature)
                    return true;
            }
            return false;
        }

        void warnDefectIfNew(const ItemDefect& defect, uint32_t* warned, int32_t& warnedCount)
        {
            const uint32_t signature = defectSignature(defect);
            if (containsSignature(signature, warned, warnedCount))
                return;
            LOGC(Warn, kItemCategory, "CORRUPT %s item @0x%08X (slot %d): type=%d subtype=%d - %s",
                 defectLocationName(defect.location), defect.address, defect.slot, defect.type,
                 defect.subtype, defect.reason);
            if (warnedCount < kMaxWarnedDefects)
                warned[warnedCount++] = signature;
        }

    }

    void pollItemCorruption()
    {
        static bool g_inWorld = false; // render thread only
        static int32_t g_countdown = 0; // frames until the next scan
        static uint32_t g_warned[kMaxWarnedDefects] = {};
        static int32_t g_warnedCount = 0;
        // Signatures from the previous scan. A defect must appear in two scans in a row before it
        // warns, so a torn read of the live inventory (the game thread can mutate it mid scan) does
        // not produce a false warning.
        static uint32_t g_pending[kMaxItemDefects] = {};
        static int32_t g_pendingCount = 0;

        uintptr_t gc = 0;
        uintptr_t creature = 0;
        if (!resolveLocalPlayer(gc, creature))
        {
            if (g_inWorld)
            {
                g_inWorld = false;
                g_warnedCount = 0; // forget warnings so re-entering the world re-checks
                g_pendingCount = 0;
            }
            return;
        }
        if (!g_inWorld)
        {
            g_inWorld = true;
            g_countdown = 0; // scan immediately on entering the world (catch pre-existing corruption)
        }
        if (g_countdown > 0)
        {
            --g_countdown;
            return;
        }
        g_countdown = kItemScanIntervalFrames;

        ItemDefect defects[kMaxItemDefects];
        const int32_t found = scanCorruptItems(defects, kMaxItemDefects);

        uint32_t current[kMaxItemDefects] = {};
        int32_t currentCount = 0;
        for (int32_t i = 0; i < found; ++i)
        {
            const uint32_t signature = defectSignature(defects[i]);
            if (currentCount < kMaxItemDefects)
                current[currentCount++] = signature;
            if (containsSignature(signature, g_pending, g_pendingCount))
                warnDefectIfNew(defects[i], g_warned, g_warnedCount);
        }

        g_pendingCount = currentCount;
        for (int32_t i = 0; i < currentCount; ++i)
            g_pending[i] = current[i];
    }
}