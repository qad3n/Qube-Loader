#include "features/game_events.h"
#include "mod_context.h"

#include <cstdio>

namespace exmod
{
    namespace
    {

        struct EventDef
        {
            const char* name;
            const char* note;
        };

        // Indexed by CubeEvent value, so kEventCatalog[ev].name doubles as the log/counter label.
        const EventDef kEventCatalog[CUBE_EVENT_COUNT] =
        {
            {"Startup", "loader finished loading mods"},
            {"Shutdown", "mod is being unloaded"},
            {"Frame", "once per rendered frame"},
            {"DeviceReset", "D3D9 device lost / reset"},
            {"WndProc", "a window message (swallowable)"},
            {"Attack", "local player started an attack (param = action id, param2 = selected target)"},
            {"Jump", "local player left the ground (amount = vertical velocity)"},
            {"AreaChange", "entered a new zone (param/param2 = zoneX/zoneY)"},
            {"Damaged", "local player HP dropped (amount = damage, param2 = HP left)"},
            {"EntityDamaged", "ANY creature took damage, attacker unattributed (subject = victim, param = category, amount = damage, param2 = HP left)"},
            {"Crit", "the LOCAL player landed a critical hit (detour backed)"},
            {"MenuOpen", "a tracked HUD menu opened"},
            {"MenuClose", "all tracked HUD menus closed"},
            {"LevelUp", "level increased (param/param2 = new/previous level)"},
            {"Death", "local player's health reached zero"},
            {"Respawn", "local player returned to life"},
            {"Land", "touched down (amount = vertical velocity)"},
            {"MovementChanged", "locomotion changed (param/param2 = new/previous)"},
            {"TargetChanged", "target changed (subject/param = new/previous address)"},
            {"EntitySpawn", "a creature entered range (param/param2 = category/type, amount = health)"},
            {"EntityDeath", "a tracked creature died (subject = creature, param/param2 = category/type)"},
            {"CoinsChanged", "coin total changed (param/param2 = total/delta)"},
            {"DayNight", "day/night flipped (param = day, param2 = hour)"},
            {"BuffGained", "a status effect was applied (param = type, amount = magnitude, param2 = ms)"},
            {"BuffLost", "a status effect ended (param = type)"},
            {"EquipmentChanged", "an equipment slot changed (param = slot, param2 = item type)"},
            {"SkillRankChanged", "a skill rank changed (param/param2 = index/new rank)"},
            {"AimTargetChanged", "crosshair hover target changed (subject = creature)"},
            {"EntityDespawn", "a creature left range (subject = address, param/param2 = category/type)"},
            {"PetSummoned", "the player's pet was created (subject = pet)"},
            {"PetDied", "the player's pet was killed (subject = pet)"},
            {"PetDismissed", "the player's pet was destroyed (subject = address)"},
            {"PlayerStunned", "LOCAL player's stun lock started (param = timer); also fires on hard falls/collisions"},
            {"PlayerKnockedDown", "LOCAL player was knocked down"},
            {"PlayerRecovered", "LOCAL player's stun lock ended (can act); pairs with PlayerStunned"},
            {"EntityStunned", "a nearby creature became stunned (subject = creature, param/param2 = category/type)"},
            {"EntityKnockedDown", "a nearby creature was knocked down (subject = creature, param/param2 = category/type)"},
            {"PetStunned", "the player's pet became stunned (subject = pet)"},
            {"PetKnockedDown", "the player's pet was knocked down (subject = pet)"},
            {"EntitySelected", "selected an entity (R / use) (subject = target, param = kind, param2 = typeByte)"},
            {"EntityRecovered", "a nearby creature's stun lock ended (subject = creature, param/param2 = category/type)"},
            {"PetRecovered", "the player's pet's stun lock ended (subject = pet)"},
            {"AbilityUsed", "used a hotbar ability 1-5 (param = ability id, param2 = cooldown ms)"},
            {"ItemPickup", "picked up an item (E) (subject = type, param = subtype, param2 = stack)"}
        };

        bool inRange(int index)
        {
            return index >= 0 && index < CUBE_EVENT_COUNT;
        }

        // Events that fire continuously from passive play (looking around, walking, creatures
        // streaming in/out, combat damage ticks) would flood the log, so their console echo starts
        // off. The Events tab still counts and lists every one, and each is toggleable there; the
        // discrete milestones below stay echoed so the log still demonstrates events reaching it.
        bool noisyByDefault(int index)
        {
            switch (index)
            {
                case CUBE_EVENT_PLAYER_ATTACK:
                case CUBE_EVENT_ENTITY_DAMAGED:
                case CUBE_EVENT_MOVEMENT_CHANGED:
                case CUBE_EVENT_TARGET_CHANGED:
                case CUBE_EVENT_AIM_TARGET_CHANGED:
                case CUBE_EVENT_ENTITY_SPAWN:
                case CUBE_EVENT_ENTITY_DESPAWN:
                case CUBE_EVENT_BUFF_GAINED:
                case CUBE_EVENT_BUFF_LOST:
                    return true;
                default:
                    return false;
            }
        }
    }

    GameEvents& gameEvents()
    {
        static GameEvents g_gameEvents;
        return g_gameEvents;
    }

    GameEvents::GameEvents()
    {
        for (int i = 0; i < CUBE_EVENT_COUNT; ++i)
            m_console[i] = !noisyByDefault(i);
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
        pushLine(kEventCatalog[index].name, detail);

        if (m_console[index])
            cubeLogf(g_api, CUBE_LOG_INFO, "example_mod: %s%s%s (#%u)", kEventCatalog[index].name,
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
        return inRange(index) ? kEventCatalog[index].name : "";
    }

    const char* GameEvents::noteAt(int index) const
    {
        return inRange(index) ? kEventCatalog[index].note : "";
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

