#pragma once
// The Mod object (accessor factory + event/hook storage), the detail trampolines, and the
// CUBE_MOD entry-point macro. Depends on every accessor header.

#include "cube/common.hpp"
#include "cube/hero.hpp"
#include "cube/world.hpp"
#include "cube/pet.hpp"
#include "cube/view.hpp"
#include "cube/items.hpp"
#include "cube/session.hpp"
#include "cube/entity.hpp"
#include "cube/selection.hpp"
#include "cube/hookcall.hpp"
#include "cube/logger.hpp"
#include "cube/events.hpp"

namespace cube
{
    class Mod
    {
    public:
        Logger log;

        // eventListener OBSERVES (runs after a detected change); eventHook INTERCEPTS (runs on the game
        // thread, can cancel/mutate/override). Both auto-clean on unload.
        EventListener eventListener;
        EventHook eventHook;

        Mod() : eventListener(this), eventHook(this) {}

        // High-level game objects - the whole surface a mod needs.
        Hero hero() const { return Hero(m_api); }
        Player player() const { return Hero(m_api); }
        Combat combat() const { return Combat(m_api); }
        World world() const { return World(m_api); }
        Pet pet() const { return Pet(m_api); }
        Camera camera() const { return Camera(m_api); }
        Display display() const { return Display(m_api); }
        Audio audio() const { return Audio(m_api); }
        Session session() const { return Session(m_api); }
        Ui ui() const { return Ui(m_api); }
        Selection selection() const { return Selection(m_api); }
        // The most recently picked-up item (E key). present()==false until the first pickup.
        // Item.getStack() is the count picked up; the item base address is 0 (transient staging copy).
        Item lastPickup() const
        {
            CubeItem data = {};
            data.structSize = sizeof(CubeItem);
            if (m_api)
                m_api->pickup.getLast(m_api, &data);
            return Item(data, m_api);
        }

        std::vector<Entity> entities() const { return entitiesOf(m_api); }
        bool target(Entity& out) const { return targetOf(m_api, out); }
        bool aimTarget(Entity& out) const { return aimTargetOf(m_api, out); }

        std::vector<Item> equipment() const { return equipmentOf(m_api); }
        std::vector<Item> inventory() const { return inventoryOf(m_api); }
        // The stock / inventory of any creature by address (a merchant's wares).
        std::vector<Item> stock(unsigned creatureAddress) const { return stockOf(m_api, creatureAddress); }
        // Resolve any (type, subtype) to its item name (the full item directory).
        const char* itemName(int type, int subtype) const { return cube::itemName(m_api, type, subtype); }
        std::vector<int> skills() const { return skillsOf(m_api); }
        std::vector<AbilityCooldown> abilityCooldowns() const { return abilityCooldownsOf(m_api); }
        bool setAbilityCooldown(int abilityId, int remainingMs) const { return m_api && m_api->skills.setCooldown(m_api, abilityId, remainingMs) != 0; }
        int clearAbilityCooldowns() const { return m_api ? m_api->skills.clearCooldowns(m_api) : 0; }
        std::vector<Buff> buffs() const { return buffsOf(m_api); }
        std::vector<Structure> structures() const { return structuresOf(m_api); }

        Environment environment() const
        {
            return m_api ? static_cast<Environment>(m_api->environment) : Environment::Client;
        }
        bool isClient() const { return environment() == Environment::Client; }
        bool isServer() const { return environment() == Environment::Server; }

        // Freeze/unfreeze the game's own input (movement + camera + cursor) so the player cannot act
        // through an open overlay. Client only.
        bool setInputBlocked(bool blocked) const
        {
            return m_api && m_api->input.setBlocked(m_api, blocked ? 1 : 0) != 0;
        }

        // Guarded memory access: a bad/stale address returns your fallback instead of crashing. Prefer
        // the typed accessors above; use these when you hold a raw address (HookCall self/target, etc).
        template <typename T>
        T read(unsigned address, T fallback = T{}) const
        {
            T value = fallback;
            if (!m_api || m_api->mem.read(m_api, address, &value, static_cast<unsigned>(sizeof(T))) == 0)
                return fallback;
            return value;
        }

        bool readable(unsigned address, unsigned size) const
        {
            return m_api && m_api->mem.readable(m_api, address, size) != 0;
        }

        template <typename T>
        bool write(unsigned address, const T& value) const
        {
            return m_api && m_api->mem.write(m_api, address, &value, static_cast<unsigned>(sizeof(T))) != 0;
        }

        unsigned rebase(unsigned staticAddress) const
        {
            return m_api ? m_api->mem.rebase(m_api, staticAddress) : 0;
        }

        // Advanced escape hatch to the raw C ABI.
        const CubeApi* raw() const { return m_api; }

        // Declare this mod's dispatch priority. Higher priority runs LAST in every event/hook reduce,
        // so it gets the final say on last-writer-wins returns; ties keep load order. Call this in the
        // mod body (it is read once at load). Changing it at runtime has no effect on dispatch order.
        void setPriority(int priority) { m_priority = priority; }
        int priority() const { return m_priority; }

        // Internal: wired by the CUBE_MOD entry point and the event trampoline.
        void bind(const CubeApi* api)
        {
            m_api = api;
            log = Logger(api);
        }

    private:
        friend class EventListener;
        friend class EventHook;

        void dispatch(EventArgs* args)
        {
            const int index = static_cast<int>(args->event);
            if (index < 0 || index >= CUBE_EVENT_COUNT)
                return;
            // Snapshot the handler list before calling: a handler may add or remove handlers for
            // this same event (a common one-shot pattern), which would invalidate a live iteration.
            const std::vector<std::function<void(EventArgs*)>> handlers = m_handlers[index];
            for (const std::function<void(EventArgs*)>& fn : handlers)
                fn(args);
        }

        // Event-listener primitive: store a handler and (once per event) subscribe the trampoline,
        // remembering the token so the whole event can be unsubscribed later.
        void addEventHandler(Event event, std::function<void(EventArgs*)> fn)
        {
            const int index = static_cast<int>(event);
            if (index < 0 || index >= CUBE_EVENT_COUNT)
                return;
            m_handlers[index].push_back(std::move(fn));
            if (m_api && !m_subscribed[index])
            {
                const uint32_t token = m_api->events.subscribe(m_api, static_cast<CubeEvent>(event), &trampoline, nullptr);
                if (token)
                {
                    m_subscribed[index] = true;
                    m_eventTokens[index] = token;
                }
            }
        }

        // Drop all of this mod's handlers for an event and unsubscribe the trampoline. Returns true
        // if the event was subscribed. Re-adding a handler later re-subscribes cleanly.
        bool removeEventHandlers(Event event)
        {
            const int index = static_cast<int>(event);
            if (index < 0 || index >= CUBE_EVENT_COUNT)
                return false;
            m_handlers[index].clear();
            const bool had = m_subscribed[index];
            if (had && m_api && m_eventTokens[index])
                m_api->events.unsubscribe(m_api, m_eventTokens[index]);
            m_subscribed[index] = false;
            m_eventTokens[index] = 0;
            return had;
        }

        // Event-hook primitives: mirror the listener but through the api->hooks sub-api.
        void addHookHandler(Hook hook, std::function<void(HookCall&)> fn)
        {
            const int index = static_cast<int>(hook);
            if (index < 0 || index >= CUBE_HOOK_COUNT)
                return;
            m_hookHandlers[index].push_back(std::move(fn));
            if (m_api && !m_hookSubscribed[index])
            {
                m_hookSubscribed[index] = true;
                m_api->hooks.on(m_api, static_cast<CubeHook>(hook), &hookTrampoline, nullptr);
            }
        }

        bool addRawHookHandler(unsigned address, CallConv cc, int argCount, std::function<void(HookCall&)> fn)
        {
            if (!m_api || !address)
                return false;
            std::vector<std::function<void(HookCall&)>>& handlers = m_rawHandlers[address];
            if (handlers.empty())
            {
                // First handler for this address: install the loader-side raw hook FIRST, keeping
                // the map entry only if it succeeded (else a failed install leaves a stale handler).
                if (m_api->hooks.onRaw(m_api, address, static_cast<CubeCallConv>(cc), argCount,
                                       &hookTrampoline, nullptr) == 0)
                {
                    m_rawHandlers.erase(address);
                    return false;
                }
            }
            handlers.push_back(std::move(fn));
            return true;
        }

        bool addRawDetourHandler(unsigned address, void* detour, void** trampoline)
        {
            return m_api && m_api->hooks.installRawDetour(m_api, address, detour, trampoline) != 0;
        }

        bool removeHookHandlers(Hook hook)
        {
            const int index = static_cast<int>(hook);
            if (index >= 0 && index < CUBE_HOOK_COUNT)
            {
                m_hookHandlers[index].clear();
                m_hookSubscribed[index] = false;
            }
            return m_api && m_api->hooks.off(m_api, static_cast<CubeHook>(hook)) != 0;
        }

        bool removeRawHookHandlers(unsigned address)
        {
            m_rawHandlers.erase(address);
            return m_api && m_api->hooks.removeRaw(m_api, address) != 0;
        }

        unsigned builtinHookTarget(Hook hook) const
        {
            return m_api ? m_api->hooks.builtinTarget(m_api, static_cast<CubeHook>(hook)) : 0;
        }

        void dispatchHook(CubeHookCall* raw)
        {
            HookCall call(raw, m_api);
            // Snapshot before calling (see dispatch): a handler may (un)subscribe mid-dispatch.
            if (raw->hook == CUBE_HOOK_RAW)
            {
                if (m_rawHandlers.count(raw->address) == 0)
                    return;
                const std::vector<std::function<void(HookCall&)>> handlers = m_rawHandlers[raw->address];
                for (const std::function<void(HookCall&)>& fn : handlers)
                    fn(call);
                return;
            }
            const int index = raw->hook;
            if (index < 0 || index >= CUBE_HOOK_COUNT)
                return;
            const std::vector<std::function<void(HookCall&)>> handlers = m_hookHandlers[index];
            for (const std::function<void(HookCall&)>& fn : handlers)
                fn(call);
        }

        static void CUBE_CALL trampoline(EventArgs* args);
        static void CUBE_CALL hookTrampoline(CubeHookCall* call);

        const CubeApi* m_api = nullptr;
        int m_priority = 0;
        std::vector<std::function<void(EventArgs*)>> m_handlers[CUBE_EVENT_COUNT];
        bool m_subscribed[CUBE_EVENT_COUNT] = {};
        uint32_t m_eventTokens[CUBE_EVENT_COUNT] = {};
        std::vector<std::function<void(HookCall&)>> m_hookHandlers[CUBE_HOOK_COUNT];
        bool m_hookSubscribed[CUBE_HOOK_COUNT] = {};
        std::map<unsigned, std::vector<std::function<void(HookCall&)>>> m_rawHandlers;
    };

    // Out-of-line EventListener / EventHook method definitions (Mod is now a complete type).
    inline void EventListener::on(Event event, std::function<void()> fn)
    {
        m_mod->addEventHandler(event, [fn = std::move(fn)](EventArgs*) { fn(); });
    }

    inline void EventListener::onRaw(Event event, std::function<void(EventArgs&)> fn)
    {
        m_mod->addEventHandler(event, [fn = std::move(fn)](EventArgs* args) { fn(*args); });
    }

    inline bool EventListener::remove(Event event)
    {
        return m_mod->removeEventHandlers(event);
    }

    inline void EventListener::onItemPickup(std::function<void(const Item&)> fn)
    {
        Mod* mod = m_mod;
        onRaw(Event::ItemPickup, [mod, fn = std::move(fn)](EventArgs&) { fn(mod->lastPickup()); });
    }

    inline void EventHook::on(Hook hook, std::function<void(HookCall&)> fn)
    {
        m_mod->addHookHandler(hook, std::move(fn));
    }

    inline bool EventHook::remove(Hook hook)
    {
        return m_mod->removeHookHandlers(hook);
    }

    inline bool EventHook::raw(unsigned address, CallConv cc, int argCount, std::function<void(HookCall&)> fn)
    {
        return m_mod->addRawHookHandler(address, cc, argCount, std::move(fn));
    }

    inline bool EventHook::rawDetour(unsigned address, void* detour, void** trampoline)
    {
        return m_mod->addRawDetourHandler(address, detour, trampoline);
    }

    inline bool EventHook::removeRaw(unsigned address)
    {
        return m_mod->removeRawHookHandlers(address);
    }

    inline unsigned EventHook::builtinTarget(Hook hook) const
    {
        return m_mod->builtinHookTarget(hook);
    }

    inline Mod& mod()
    {
        static Mod g_mod;
        return g_mod;
    }

    inline void CUBE_CALL Mod::trampoline(EventArgs* args)
    {
        mod().dispatch(args);
    }

    inline void CUBE_CALL Mod::hookTrampoline(CubeHookCall* call)
    {
        mod().dispatchHook(call);
    }

    namespace detail
    {

        inline CubeModInfo& info()
        {
            static CubeModInfo g_info;
            return g_info;
        }

        inline CubeModInfo* boot(const CubeApi* api, const char* name, const char* version,
                                 const char* author, void (*entry)(Mod&))
        {
            // Reject a loader whose ABI does not match what this mod was built against, so a stale
            // binary fails cleanly at load instead of reading shifted struct offsets at runtime.
            if (!api || api->abiVersion != CUBE_ABI_VERSION)
                return nullptr;
            mod().bind(api);
            info().structSize = sizeof(CubeModInfo);
            info().name = name;
            info().version = version;
            info().author = author;
            info().priority = 0;
            entry(mod());
            info().priority = mod().priority();
            return &info();
        }

    }

}

#define CUBE_MOD(modName, modVersion, modAuthor) \
    static void cube_mod_main(cube::Mod& mod); \
    CUBE_MOD_API CubeModInfo* CUBE_CALL CubeMod_Init(const CubeApi* api) \
    { \
        return cube::detail::boot(api, modName, modVersion, modAuthor, &cube_mod_main); \
    } \
    static void cube_mod_main(cube::Mod& mod)
