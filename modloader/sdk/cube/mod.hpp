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
#include "cube/config.hpp"
#include "cube/storage.hpp"
#include "cube/services.hpp"
#include "cube/locale.hpp"
#include "cube/assets.hpp"
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
        // Per-mod persistence: config() is user-editable settings (<stem>.ini); storage() is mod-owned
        // binary save data. Both keyed by this mod's DLL stem (stable and available even in init,
        // unlike the manifest id), so a mod's own files stay put regardless of the id it declares.
        Config config() const { return Config(m_api); }
        Storage storage() const { return Storage(m_api); }
        // Per-mod localization: translate keys against this mod's <dllDir>/lang/<stem>/<locale>.ini.
        Locale locale() const { return Locale(m_api); }
        // Inter-mod ecosystem: publish/resolve a named shared service and message another mod by its id.
        // Resolve peers at eventListener().onReady() (every mod has loaded by then).
        Services services() const { return Services(m_api, const_cast<Mod*>(this)); }
        // Asset overrides: replace a game asset (texture/model/...) by its filename key. Requires the
        // Assets capability and a compatible game build.
        Assets assets() const { return Assets(m_api); }
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

        // Declare a stable machine id (unique across mods) - the key for this mod's config, storage,
        // services, and dependency references. Defaults to the DLL filename stem if left unset.
        void setId(const char* id) { m_id = id; }
        const char* id() const { return m_id; }

        // Declare the powers this mod uses (OR Capability flags). Leave unset for unrestricted access.
        void setCapabilities(unsigned capabilities) { m_capabilities = capabilities; }
        unsigned capabilities() const { return m_capabilities; }

        // Declare an optional home/version URL. The loader only reports it in the load banner; it does
        // no network access unless the user opts in to update checking.
        void setUpdateUrl(const char* url) { m_updateUrl = url; }
        const char* updateUrl() const { return m_updateUrl; }

        // Declare a dependency on another mod by id. A hard dep refuses to load this mod when unmet.
        void dependsOn(const char* id, const char* minVersion = nullptr, bool hard = true)
        {
            CubeModDep dep;
            dep.id = id;
            dep.minVersion = minVersion;
            dep.hard = hard ? 1 : 0;
            m_deps.push_back(dep);
        }

        // Internal: wired by the CUBE_MOD entry point and the event trampoline.
        void bind(const CubeApi* api)
        {
            m_api = api;
            log = Logger(api);
        }

        // Internal: the null-terminated dep array handed to the loader via CubeModInfo (read once at boot).
        const CubeModDep* depsData()
        {
            if (m_deps.empty())
                return nullptr;
            if (!m_depsTerminated)
            {
                CubeModDep terminator;
                terminator.id = nullptr;
                terminator.minVersion = nullptr;
                terminator.hard = 0;
                m_deps.push_back(terminator);
                m_depsTerminated = true;
            }
            return m_deps.data();
        }

    private:
        friend class EventListener;
        friend class EventHook;
        friend class Services;

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

        // Inter-mod message receiver: store a handler and (once) subscribe the loader-side trampoline,
        // fanning every directed message out to all handlers - the messaging analogue of addEventHandler.
        void addMessageHandler(std::function<void(Message&)> fn)
        {
            m_messageHandlers.push_back(std::move(fn));
            if (m_api && !m_messageSubscribed)
            {
                const uint32_t token = m_api->services.onMessage(m_api, &messageTrampoline, nullptr);
                if (token)
                {
                    m_messageSubscribed = true;
                    m_messageToken = token;
                }
            }
        }

        void dispatchMessage(CubeMessageArgs* raw)
        {
            Message msg(raw);
            // Snapshot before calling (see dispatch): a handler may (un)register handlers mid-dispatch.
            const std::vector<std::function<void(Message&)>> handlers = m_messageHandlers;
            for (const std::function<void(Message&)>& fn : handlers)
                fn(msg);
        }

        static void CUBE_CALL trampoline(EventArgs* args);
        static void CUBE_CALL hookTrampoline(CubeHookCall* call);
        static void CUBE_CALL messageTrampoline(const CubeApi* api, CubeMessageArgs* args, void* user);

        const CubeApi* m_api = nullptr;
        int m_priority = 0;
        const char* m_id = nullptr;
        unsigned m_capabilities = 0;
        const char* m_updateUrl = nullptr;
        std::vector<CubeModDep> m_deps;
        bool m_depsTerminated = false;
        std::vector<std::function<void(EventArgs*)>> m_handlers[CUBE_EVENT_COUNT];
        bool m_subscribed[CUBE_EVENT_COUNT] = {};
        uint32_t m_eventTokens[CUBE_EVENT_COUNT] = {};
        std::vector<std::function<void(HookCall&)>> m_hookHandlers[CUBE_HOOK_COUNT];
        bool m_hookSubscribed[CUBE_HOOK_COUNT] = {};
        std::map<unsigned, std::vector<std::function<void(HookCall&)>>> m_rawHandlers;
        std::vector<std::function<void(Message&)>> m_messageHandlers;
        bool m_messageSubscribed = false;
        uint32_t m_messageToken = 0;
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

    inline void Services::onMessage(std::function<void(Message&)> fn) const
    {
        if (m_mod)
            m_mod->addMessageHandler(std::move(fn));
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

    inline void CUBE_CALL Mod::messageTrampoline(const CubeApi*, CubeMessageArgs* args, void*)
    {
        mod().dispatchMessage(args);
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
            // Reject a loader OLDER than what this mod was built against: it would be missing sub-apis
            // or struct fields the mod expects. A newer loader is fine because ABI growth is additive
            // (appended members keep existing offsets), so the loader still serves this mod's prefix.
            if (!api)
                return nullptr;
            if (api->abiVersion < CUBE_ABI_VERSION)
            {
                // log sits at a fixed offset behind the version header, so it is callable on any loader.
                cubeLogf(api, CUBE_LOG_ERROR, "%s: built against ABI v%u but the loader serves v%u; update the loader",
                         name, static_cast<unsigned>(CUBE_ABI_VERSION), static_cast<unsigned>(api->abiVersion));
                return nullptr;
            }
            mod().bind(api);
            info().structSize = sizeof(CubeModInfo);
            info().name = name;
            info().version = version;
            info().author = author;
            info().priority = 0;
            entry(mod());
            info().priority = mod().priority();
            // Manifest fields resolved from what the mod declared in its body. requiredAbi is stamped
            // automatically to the ABI this mod compiled against, so the loader can range-check it.
            info().requiredAbi = CUBE_ABI_VERSION;
            info().id = mod().id();
            info().capabilities = mod().capabilities();
            info().deps = mod().depsData();
            info().updateUrl = mod().updateUrl();
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
