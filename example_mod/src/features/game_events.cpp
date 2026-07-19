#include "features/game_events.h"
#include "mod_context.h"

#include <cstdio>

namespace exmod
{
    namespace
    {

        // Indexed by CubeEvent value; the name doubles as the log/counter label. Per-event payload
        // semantics (subject/param/param2/amount) are documented in the SDK event catalog (enums.h).
        const char* const kEventNames[CUBE_EVENT_COUNT] =
        {
            "Startup", "Shutdown", "Frame", "DeviceReset", "WndProc",
            "Attack", "Jump", "AreaChange", "Damaged", "EntityDamaged",
            "Crit", "MenuOpen", "MenuClose", "LevelUp", "Death",
            "Respawn", "Land", "MovementChanged", "TargetChanged", "EntitySpawn",
            "EntityDeath", "CoinsChanged", "DayNight", "BuffGained", "BuffLost",
            "EquipmentChanged", "SkillRankChanged", "AimTargetChanged", "EntityDespawn", "PetSummoned",
            "PetDied", "PetDismissed", "PlayerStunned", "PlayerKnockedDown", "PlayerRecovered",
            "EntityStunned", "EntityKnockedDown", "PetStunned", "PetKnockedDown", "EntitySelected",
            "EntityRecovered", "PetRecovered", "AbilityUsed", "ItemPickup", "PlayerRoll",
            "Ready", "WorldEnter", "WorldExit"
        };

        bool inRange(int index)
        {
            return index >= 0 && index < CUBE_EVENT_COUNT;
        }

    }

    GameEvents& gameEvents()
    {
        static GameEvents g_gameEvents;
        return g_gameEvents;
    }

    GameEvents::GameEvents()
    {
        // Every event echoes to the console by default; each is still toggleable in the Events tab
        // (the high-frequency ones like MOVEMENT_CHANGED / ENTITY_DAMAGED can be muted there).
        for (int i = 0; i < CUBE_EVENT_COUNT; ++i)
            m_console[i] = true;
    }

    void GameEvents::install(cube::EventListener& listener)
    {
        m_listener = &listener;
        // Each handler takes the enriched overload and logs the real per event data the loader passed,
        // so the Events tab shows what a mod can actually read and branch on inside a listener.
        listener.onAttack([this](int actionId, unsigned selectedTarget)
        {
            char detail[40];
            std::snprintf(detail, sizeof(detail), "action 0x%02X -> 0x%08X", actionId, selectedTarget);
            record(CUBE_EVENT_PLAYER_ATTACK, detail);
        });
        listener.onAbilityUsed([this](int abilityId, int cooldownMs)
        {
            char detail[48];
            const char* name = cube::catalog::nameOr(g_api, CUBE_CATALOG_ABILITY, abilityId, "ability");
            std::snprintf(detail, sizeof(detail), "%s (#%d) cd %dms", name, abilityId, cooldownMs);
            record(CUBE_EVENT_ABILITY_USED, detail);
        });
        listener.onJump([this](float verticalVelocity)
        {
            char detail[24];
            std::snprintf(detail, sizeof(detail), "vz %.1f", verticalVelocity);
            record(CUBE_EVENT_PLAYER_JUMP, detail);
        });
        listener.onAreaChange([this](int zoneX, int zoneY)
        {
            char detail[32];
            std::snprintf(detail, sizeof(detail), "zone %d,%d", zoneX, zoneY);
            record(CUBE_EVENT_AREA_CHANGE, detail);
        });
        listener.onDamaged([this](float damage, int remainingHealth)
        {
            char detail[32];
            std::snprintf(detail, sizeof(detail), "-%.0f hp (%d left)", damage, remainingHealth);
            record(CUBE_EVENT_PLAYER_DAMAGED, detail);
        });
        listener.onEntityDamaged([this](unsigned victim, float damage, int remainingHealth, int category)
        {
            char detail[56];
            std::snprintf(detail, sizeof(detail), "0x%08X cat %d -%.0f hp (%d left)", victim, category, damage, remainingHealth);
            record(CUBE_EVENT_ENTITY_DAMAGED, detail);
        });
        listener.onCrit([this] { record(CUBE_EVENT_PLAYER_CRIT); });
        listener.onLevelUp([this](int newLevel, int previousLevel)
        {
            char detail[32];
            std::snprintf(detail, sizeof(detail), "%d -> %d", previousLevel, newLevel);
            record(CUBE_EVENT_PLAYER_LEVELUP, detail);
        });
        listener.onMenuOpen([this](int openMask)
        {
            char detail[24];
            std::snprintf(detail, sizeof(detail), "mask 0x%X", openMask);
            record(CUBE_EVENT_MENU_OPEN, detail);
        });
        listener.onMenuClose([this] { record(CUBE_EVENT_MENU_CLOSE); });
        listener.onDeath([this] { record(CUBE_EVENT_PLAYER_DEATH); });
        listener.onRespawn([this] { record(CUBE_EVENT_PLAYER_RESPAWN); });
        listener.onLand([this](float verticalVelocity)
        {
            char detail[24];
            std::snprintf(detail, sizeof(detail), "vz %.1f", verticalVelocity);
            record(CUBE_EVENT_PLAYER_LAND, detail);
        });
        listener.onRoll([this](float verticalVelocity)
        {
            char detail[24];
            std::snprintf(detail, sizeof(detail), "vz %.1f", verticalVelocity);
            record(CUBE_EVENT_PLAYER_ROLL, detail);
        });
        listener.onMovementChanged([this](cube::Movement current, cube::Movement previous)
        {
            char detail[40];
            std::snprintf(detail, sizeof(detail), "%s <- %s", cube::movementName(current), cube::movementName(previous));
            record(CUBE_EVENT_MOVEMENT_CHANGED, detail);
        });
        listener.onTargetChanged([this](unsigned target, unsigned previousTarget)
        {
            char detail[48];
            std::snprintf(detail, sizeof(detail), "0x%08X <- 0x%08X", target, previousTarget);
            record(CUBE_EVENT_TARGET_CHANGED, detail);
        });
        listener.onEntitySpawn([this](unsigned creature, int category, int type, float health)
        {
            char detail[56];
            std::snprintf(detail, sizeof(detail), "0x%08X cat %d type %d hp %.0f", creature, category, type, health);
            record(CUBE_EVENT_ENTITY_SPAWN, detail);
        });
        listener.onEntityDeath([this](unsigned creature, int category, int type)
        {
            char detail[48];
            std::snprintf(detail, sizeof(detail), "0x%08X cat %d type %d", creature, category, type);
            record(CUBE_EVENT_ENTITY_DEATH, detail);
        });
        listener.onCoinsChanged([this](int newTotal, int delta)
        {
            char detail[32];
            std::snprintf(detail, sizeof(detail), "%d (%+d)", newTotal, delta);
            record(CUBE_EVENT_COINS_CHANGED, detail);
        });
        listener.onDayNight([this](bool isDay, int hour)
        {
            char detail[32];
            std::snprintf(detail, sizeof(detail), "%s @ %02dh", isDay ? "day" : "night", hour);
            record(CUBE_EVENT_DAY_NIGHT, detail);
        });
        listener.onBuffGained([this](int type, float magnitude, int remainingMs)
        {
            char detail[40];
            std::snprintf(detail, sizeof(detail), "type %d x%.1f %dms", type, magnitude, remainingMs);
            record(CUBE_EVENT_BUFF_GAINED, detail);
        });
        listener.onBuffLost([this](int type)
        {
            char detail[24];
            std::snprintf(detail, sizeof(detail), "type %d", type);
            record(CUBE_EVENT_BUFF_LOST, detail);
        });
        listener.onEquipmentChanged([this](int slot, int itemType)
        {
            char detail[32];
            std::snprintf(detail, sizeof(detail), "slot %d type %d", slot, itemType);
            record(CUBE_EVENT_EQUIPMENT_CHANGED, detail);
        });
        listener.onSkillRankChanged([this](int index, int newRank)
        {
            char detail[32];
            std::snprintf(detail, sizeof(detail), "skill %d rank %d", index, newRank);
            record(CUBE_EVENT_SKILL_RANK_CHANGED, detail);
        });
        listener.onAimTargetChanged([this](unsigned) { record(CUBE_EVENT_AIM_TARGET_CHANGED); });
        listener.onEntityDespawn([this](unsigned creature, int category, int type)
        {
            char detail[48];
            std::snprintf(detail, sizeof(detail), "0x%08X cat %d type %d", creature, category, type);
            record(CUBE_EVENT_ENTITY_DESPAWN, detail);
        });
        listener.onPetSummoned([this](unsigned) { record(CUBE_EVENT_PET_SUMMONED); });
        listener.onPetDied([this](unsigned) { record(CUBE_EVENT_PET_DIED); });
        listener.onPetDismissed([this](unsigned) { record(CUBE_EVENT_PET_DISMISSED); });
        listener.onStunned([this](int hitStun)
        {
            char detail[24];
            std::snprintf(detail, sizeof(detail), "timer %d", hitStun);
            record(CUBE_EVENT_PLAYER_STUNNED, detail);
        });
        listener.onKnockedDown([this] { record(CUBE_EVENT_PLAYER_KNOCKED_DOWN); });
        listener.onRecovered([this] { record(CUBE_EVENT_PLAYER_RECOVERED); });
        listener.onEntityStunned([this](unsigned creature, int category, int type)
        {
            char detail[48];
            std::snprintf(detail, sizeof(detail), "0x%08X cat %d type %d", creature, category, type);
            record(CUBE_EVENT_ENTITY_STUNNED, detail);
        });
        listener.onEntityRecovered([this](unsigned creature, int category, int type)
        {
            char detail[48];
            std::snprintf(detail, sizeof(detail), "0x%08X cat %d type %d", creature, category, type);
            record(CUBE_EVENT_ENTITY_RECOVERED, detail);
        });
        listener.onEntityKnockedDown([this](unsigned creature, int category, int type)
        {
            char detail[48];
            std::snprintf(detail, sizeof(detail), "0x%08X cat %d type %d", creature, category, type);
            record(CUBE_EVENT_ENTITY_KNOCKED_DOWN, detail);
        });
        listener.onPetStunned([this](unsigned) { record(CUBE_EVENT_PET_STUNNED); });
        listener.onPetRecovered([this](unsigned) { record(CUBE_EVENT_PET_RECOVERED); });
        listener.onPetKnockedDown([this](unsigned) { record(CUBE_EVENT_PET_KNOCKED_DOWN); });
        listener.onEntitySelected([this](unsigned target, cube::SelectionKind kind, int typeByte)
        {
            char detail[48];
            std::snprintf(detail, sizeof(detail), "0x%08X %s t0x%02X", target, cube::selectionKindName(kind), typeByte);
            record(CUBE_EVENT_ENTITY_SELECTED, detail);
        });
        listener.onItemPickup([this](const cube::Item& item)
        {
            char detail[64];
            std::snprintf(detail, sizeof(detail), "%s x%d (type %d)", item.getName(), item.getStack(), item.getType());
            record(CUBE_EVENT_ITEM_PICKUP, detail);
        });
        // Lifecycle edges (ABI 22): READY fires once after all mods load + deps resolve; WorldEnter/Exit
        // on the title/menu <-> in-world crossing (distinct from AreaChange's zone-to-zone).
        listener.onReady([this] { record(CUBE_EVENT_READY); });
        listener.onWorldEnter([this] { record(CUBE_EVENT_WORLD_ENTER); });
        listener.onWorldExit([this] { record(CUBE_EVENT_WORLD_EXIT); });
    }

    void GameEvents::record(CubeEvent event)
    {
        record(event, nullptr);
    }

    void GameEvents::record(CubeEvent event, const char* detail)
    {
        const int index = static_cast<int>(event);
        if (!inRange(index))
            return;

        ++m_counts[index];
        pushLine(kEventNames[index], detail);

        if (m_console[index])
            cubeLogf(g_api, CUBE_LOG_INFO, "example_mod: %s%s%s (#%u)", kEventNames[index],
                detail ? " " : "", detail ? detail : "", m_counts[index]);
    }

    void GameEvents::pushLine(const char* name, const char* detail)
    {
        ++m_seq;
        if (detail && detail[0])
            std::snprintf(m_lines[m_head], kLogLineLen, "%-5u %s %s", m_seq, name, detail);
        else
            std::snprintf(m_lines[m_head], kLogLineLen, "%-5u %s", m_seq, name);
        m_head = (m_head + 1) % kLogSize;
        if (m_count < kLogSize)
            ++m_count;
    }

    unsigned GameEvents::countAt(int index) const
    {
        return inRange(index) ? m_counts[index] : 0;
    }

    const char* GameEvents::nameAt(int index) const
    {
        return inRange(index) ? kEventNames[index] : "";
    }

    bool& GameEvents::consoleEnabled(int index)
    {
        static bool g_discard = false;
        return inRange(index) ? m_console[index] : g_discard;
    }

    const char* GameEvents::logLineAt(int oldestFirst) const
    {
        const int start = (m_head - m_count + kLogSize) % kLogSize;
        return m_lines[(start + oldestFirst) % kLogSize];
    }

    void GameEvents::clearLog()
    {
        m_head = 0;
        m_count = 0;
    }

    void GameEvents::setAreaChangeListening(bool listening)
    {
        if (!m_listener || listening == m_areaChangeListening)
            return;
        if (listening)
            m_listener->onAreaChange([this](int zoneX, int zoneY)
            {
                char detail[32];
                std::snprintf(detail, sizeof(detail), "zone %d,%d", zoneX, zoneY);
                record(CUBE_EVENT_AREA_CHANGE, detail);
            });
        else
            m_listener->remove(cube::Event::AreaChange);
        m_areaChangeListening = listening;
    }
}

