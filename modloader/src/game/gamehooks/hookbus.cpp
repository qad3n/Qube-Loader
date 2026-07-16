#include "game/gamehooks/gamehooks.h"
#include "game/gamehooks/builtin/builtin.h"
#include "game/gamehooks/rawpool.h"
#include "game/attackwatch.h"
#include "hooks/detour.h"
#include "modloader/core/owner_name.h"
#include "modloader/core/conflict.h"
#include "modloader/game/gameevents.h"
#include "core/log.h"
#include "core/faultguard.h"
#include "util/guard.h"
#include "util/inflight.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

// Hook bus: per-mod handler registry, multi-mod dispatch + reduce, per-hook arm/disarm refcount.
namespace game::gamehooks
{
    namespace
    {
        constexpr char kCategory[] = "gamehooks";
        constexpr int kLabelMax = 96; // stack buffer for a per-handler guard label (no heap alloc)
        using modloader::ownerName;
        using modloader::ownerPriority;
        using modloader::ownerOrder;

        struct HookSub
        {
            const CubeApi* owner;
            CubeHook hook;
            CubeHookFn fn;
            void* user;
        };

        struct RawSub
        {
            const CubeApi* owner;
            uint32_t address;
            CubeHookFn fn;
            void* user;
        };

        // Installed via installRawDetour (mod owns the detour; tracked only for removal on unload).
        struct RawDetour
        {
            const CubeApi* owner;
            uint32_t address;
        };

        std::mutex g_mutex;
        std::vector<HookSub> g_subs;
        std::vector<RawSub> g_rawSubs;
        std::vector<RawDetour> g_rawDetours;
        // Install refcount = reservations (attack/crit sampling) + real subscribers; drives arm/disarm
        // of the trampoline. Subscriber count = real mod handlers only; drives the dispatch gate. A
        // reservation therefore keeps a detour installed yet pass-through until a mod actually hooks it.
        int32_t g_installRefcount[CUBE_HOOK_COUNT] = {};
        int32_t g_subCount[CUBE_HOOK_COUNT] = {};
        bool g_armed[CUBE_HOOK_COUNT] = {};
        // Drained by unsubscribeOwner after dropping a mod's subs so no handler outlives its DLL
        // (the detour may stay armed for another mod, uncovered by disarm's own drain).
        std::atomic<int> g_dispatchInFlight{0};

        // Increment the install refcount; arm the trampoline on the 0->1 edge. Returns whether the
        // detour is installed afterward. Never touches the dispatch gate: a fresh install is pass-through
        // until a real subscriber activates it, so an observation reservation stays vanilla.
        bool acquireInstall(CubeHook hook)
        {
            bool needArm = false;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                needArm = (g_installRefcount[hook] == 0);
                ++g_installRefcount[hook];
                if (!needArm)
                    return g_armed[hook];
            }

            const bool ok = armBuiltin(hook);

            std::lock_guard<std::mutex> lock(g_mutex);
            g_armed[hook] = ok;
            if (!ok && --g_installRefcount[hook] < 0)
                g_installRefcount[hook] = 0;
            return ok;
        }

        // Decrement the install refcount by n; disarm the trampoline on the ->0 edge.
        void releaseInstall(CubeHook hook, int32_t n)
        {
            bool disarm = false;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_installRefcount[hook] -= n;
                if (g_installRefcount[hook] <= 0)
                {
                    g_installRefcount[hook] = 0;
                    if (g_armed[hook])
                    {
                        g_armed[hook] = false;
                        disarm = true;
                    }
                }
            }

            if (disarm)
                disarmBuiltin(hook);
        }

        const char* outcomeName(const CubeHookCall& call)
        {
            if (call.cancel)
                return "CANCELLED";
            if (call.overrideReturn)
                return "return-overridden";

            return "passed through";
        }

        bool validHook(CubeHook hook)
        {
            return hook >= 0 && hook < CUBE_HOOK_COUNT;
        }

        constexpr uint32_t kHookWarnCooldownFrames = 300;
        constexpr uint64_t kFnvOffset = 1469598103934665603ull;
        constexpr uint64_t kFnvPrime = 1099511628211ull;

        uint64_t hashSubject(const char* s)
        {
            uint64_t h = kFnvOffset;
            for (; s && *s; ++s)
            {
                h ^= static_cast<unsigned char>(*s);
                h *= kFnvPrime;
            }
            return h;
        }

        // Two mods overriding the same hook's return silently fight (last-writer-wins). Warn loudly,
        // throttled per (hook, owner-pair) so a per-frame combat hook does not flood the log.
        void warnContestedReturn(const char* subject, const CubeApi* first, const CubeApi* second)
        {
            const uintptr_t lo = reinterpret_cast<uintptr_t>(first < second ? first : second);
            const uintptr_t hi = reinterpret_cast<uintptr_t>(first < second ? second : first);
            const uint64_t sig = hashSubject(subject) ^ static_cast<uint64_t>(lo) ^ (static_cast<uint64_t>(hi) << 1);
            if (modloader::conflict::throttle(sig, modloader::gameevents::currentFrame(), kHookWarnCooldownFrames))
                modloader::conflict::warn("hook %s: '%s' and '%s' both override the return; last-writer-wins, the game may misbehave",
                                          subject, ownerName(first), ownerName(second));
        }

        // Order handlers low-to-high priority so the highest-priority mod runs LAST and wins the
        // last-writer-wins return reduce; within one priority a dependency (lower topological rank)
        // runs before its dependents; stable so fully-equal keys keep subscription (load) order.
        void sortByPriority(std::vector<HookSub>& matched)
        {
            std::stable_sort(matched.begin(), matched.end(), [](const HookSub& a, const HookSub& b)
            {
                if (ownerPriority(a.owner) != ownerPriority(b.owner))
                    return ownerPriority(a.owner) < ownerPriority(b.owner);
                return ownerOrder(a.owner) < ownerOrder(b.owner);
            });
        }

        // Reduce: cancel sticky OR, args chain, return last-writer-wins. Each handler guarded (no SEH
        // under mingw). `subject` names the hook, used both for the per-mod guard label (so a throwing
        // handler is attributed) and the contested-return warning.
        int32_t dispatchMatched(const std::vector<HookSub>& matched, const char* subject, CubeHookCall& call)
        {
            int32_t cancel = call.cancel;
            const CubeApi* returnSetter = nullptr;
            for (const HookSub& sub : matched)
            {
                if (faultguard::isQuarantined(sub.owner))
                    continue; // a mod disabled after a fault no longer intercepts calls
                call.cancel = cancel;
                const int32_t prevOverride = call.overrideReturn;
                const int32_t prevReturnI = call.returnI;
                const float prevReturnF = call.returnF;
                // Per-handler label names the mod + hook so a fault/exception is attributed to it.
                char label[kLabelMax];
                std::snprintf(label, sizeof(label), "mod '%s' %s hook callback", ownerName(sub.owner), subject);
                guard::tryRun(label, sub.owner, [&]()
                {
                    sub.fn(&call);
                });
                cancel |= call.cancel;

                const bool assertedReturn = call.overrideReturn && (!prevOverride || call.returnI != prevReturnI || call.returnF != prevReturnF);
                if (assertedReturn)
                {
                    if (returnSetter && returnSetter != sub.owner)
                        warnContestedReturn(subject, returnSetter, sub.owner);
                    returnSetter = sub.owner;
                }
            }
            call.cancel = cancel;
            return cancel;
        }

        // Caller holds g_mutex.
        bool anyRawSubAt(uint32_t address)
        {
            for (const RawSub& sub : g_rawSubs)
            {
                if (sub.address == address)
                    return true;
            }
            return false;
        }

        constexpr char kRawLabelFmt[] = "raw 0x%08X";
        constexpr int kRawLabelMax = 16; // "raw 0x" + 8 hex digits + null

        void formatRawLabel(uint32_t address, char* out, size_t size)
        {
            std::snprintf(out, size, kRawLabelFmt, address);
        }
    }

    const char* hookName(CubeHook hook)
    {
        switch (hook)
        {
            case CUBE_HOOK_IMPACT:
                return "IMPACT";
            case CUBE_HOOK_CRIT_ROLL:
                return "CRIT_ROLL";
            case CUBE_HOOK_MAX_HEALTH:
                return "MAX_HEALTH";
            case CUBE_HOOK_RAW:
                return "RAW";
            default:
                return "?";
        }
    }

    std::string describeOwner(const CubeApi* owner)
    {
        std::string out;
        std::lock_guard<std::mutex> lock(g_mutex);

        for (const HookSub& sub : g_subs)
        {
            if (sub.owner != owner)
                continue;
            if (!out.empty())
                out += ", ";
            out += hookName(sub.hook);
        }

        for (const RawSub& sub : g_rawSubs)
        {
            if (sub.owner != owner)
                continue;
            char label[kRawLabelMax];
            formatRawLabel(sub.address, label, sizeof(label));
            if (!out.empty())
                out += ", ";
            out += label;
        }

        return out;
    }

    void forEachSubscription(const std::function<void(const CubeApi*, const char*)>& fn)
    {
        std::lock_guard<std::mutex> lock(g_mutex);

        for (const HookSub& sub : g_subs)
            fn(sub.owner, hookName(sub.hook));

        for (const RawSub& sub : g_rawSubs)
        {
            char label[kRawLabelMax];
            formatRawLabel(sub.address, label, sizeof(label));
            fn(sub.owner, label);
        }
    }

    void armAttackWatch()
    {
        // Reserve the IMPACT detour installed for the whole session so the attack watcher can sample
        // the local player's action every tick, WITHOUT activating dispatch. With no mod subscribed the
        // detour stays pass-through (runs the original untouched) and only reads state; a mod that hooks
        // IMPACT flips it active, this reservation never does. Released at shutdown by shutdownBuiltin.
        if (!acquireInstall(CUBE_HOOK_IMPACT))
        {
            LOGC(Warn, kCategory, "attack watcher could not arm IMPACT detour; PLAYER_ATTACK falls back to polling");
            return;
        }

        attackwatch::setActive(true);
        LOGC(Debug, kCategory, "attack watcher active (IMPACT detour installed, pass-through until a mod subscribes)");
    }

    void armCritCounter()
    {
        // Reserve the CRIT_ROLL detour installed so the loader can count the local player's crits,
        // WITHOUT activating dispatch. Pass-through returns the real roll untouched and only increments
        // the counter; a mod that hooks CRIT_ROLL flips it active. Released at shutdown by shutdownBuiltin.
        if (!acquireInstall(CUBE_HOOK_CRIT_ROLL))
        {
            LOGC(Warn, kCategory, "crit counter could not arm CRIT_ROLL detour; crits will not be counted");
            return;
        }

        LOGC(Debug, kCategory, "crit counter active (CRIT_ROLL detour installed, pass-through until a mod subscribes)");
    }

    void subscribe(const CubeApi* owner, CubeHook hook, CubeHookFn fn, void* user)
    {
        if (!owner || !fn || !validHook(hook))
            return;

        // Install (arm) the trampoline first; a failed arm must not leave a subscriber registered.
        if (!acquireInstall(hook))
        {
            LOGC(Warn, kCategory, "'%s' built-in hook %s failed to arm; subscription dropped",
                 ownerName(owner), hookName(hook));
            return;
        }

        int32_t count = 0;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_subs.push_back(HookSub{owner, hook, fn, user});
            count = ++g_subCount[hook];
        }

        // A real subscriber now exists: leave pass-through and dispatch to handlers.
        setBuiltinActive(hook, true);
        LOGC(Debug, kCategory, "'%s' subscribed built-in hook %s (#%d, now %d subscriber(s))", ownerName(owner), hookName(hook), static_cast<int>(hook), count);
    }

    int32_t unsubscribeHook(const CubeApi* owner, CubeHook hook)
    {
        if (!owner || !validHook(hook))
            return 0;

        int32_t removed = 0;
        bool deactivate = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            for (size_t i = g_subs.size(); i > 0; --i)
            {
                if (g_subs[i - 1].owner == owner && g_subs[i - 1].hook == hook)
                {
                    g_subs.erase(g_subs.begin() + (i - 1));
                    ++removed;
                }
            }

            if (removed)
            {
                g_subCount[hook] -= removed;
                if (g_subCount[hook] <= 0)
                {
                    g_subCount[hook] = 0;
                    deactivate = true;
                }
            }
        }

        if (!removed)
            return 0;

        // Last real subscriber gone: back to pass-through, then drop the install hold (a reservation
        // keeps the trampoline armed; releaseInstall only disarms once nothing holds it).
        if (deactivate)
            setBuiltinActive(hook, false);
        releaseInstall(hook, removed);

        return removed;
    }

    int32_t dispatchBuiltin(CubeHook hook, CubeHookCall& call)
    {
        if (!validHook(hook))
            return 0;

        std::vector<HookSub> matched;
        std::unique_lock<std::mutex> lock(g_mutex);

        for (const HookSub& sub : g_subs)
        {
            if (sub.hook == hook)
                matched.push_back(sub);
        }

        if (matched.empty())
            return 0;

        // Bump in-flight under the lock so the drain can't observe zero between snapshot and dispatch.
        barrier::InFlight inflight(g_dispatchInFlight);
        lock.unlock();

        sortByPriority(matched);
        LOGC(Trace, kCategory, "dispatch %s self=0x%08X target=0x%08X arg0=%d -> %zu handler(s)", hookName(hook), call.self, call.target, call.argi[0], matched.size());

        const int32_t cancel = dispatchMatched(matched, hookName(hook), call);
        LOGC(Trace, kCategory, "  %s -> %s (returnI=%d returnF=%.3f)", hookName(hook), outcomeName(call), call.returnI, call.returnF);
        return cancel;
    }

    int32_t dispatchRaw(uint32_t address, CubeHookCall& call)
    {
        std::vector<HookSub> matched;
        std::unique_lock<std::mutex> lock(g_mutex);

        for (const RawSub& sub : g_rawSubs)
        {
            if (sub.address == address)
                matched.push_back(HookSub{sub.owner, CUBE_HOOK_RAW, sub.fn, sub.user});
        }

        if (matched.empty())
            return 0;

        // See dispatchBuiltin: increment under the lock so the drain serializes against this dispatch.
        barrier::InFlight inflight(g_dispatchInFlight);
        lock.unlock();

        sortByPriority(matched);
        char subject[kRawLabelMax];
        formatRawLabel(address, subject, sizeof(subject));
        LOGC(Trace, kCategory, "dispatch raw 0x%08X self=0x%08X arg0=%d -> %zu handler(s)", address, call.self, call.argi[0], matched.size());
        const int32_t cancel = dispatchMatched(matched, subject, call);
        LOGC(Trace, kCategory, "  raw 0x%08X -> %s", address, outcomeName(call));
        return cancel;
    }

    int32_t installRaw(const CubeApi* owner, uint32_t address, CubeCallConv cc, int32_t argCount, CubeHookFn fn, void* user)
    {
        if (!owner || !fn || !address)
            return 0;

        bool needPool = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            needPool = !anyRawSubAt(address); // reuse the slot if another handler already hooked it
        }

        if (needPool)
        {
            if (!rawpool::install(address, cc, argCount))
            {
                LOGC(Warn, kCategory, "'%s' raw hook 0x%08X failed (pool full or unhookable)",
                     ownerName(owner), address);
                return 0;
            }
        }
        else
        {
            // The slot's (cc, argCount) are fixed by its first registrant, so a mismatched second
            // registration is marshalled against the wrong config. Warn (do not fail) so it is visible.
            CubeCallConv slotCc = cc;
            int32_t slotArgs = argCount;
            if (rawpool::config(address, slotCc, slotArgs) && (slotCc != cc || slotArgs != argCount))
                LOGC(Warn, kCategory, "'%s' raw hook 0x%08X uses (cc %d, %d args) but the existing slot is (cc %d, %d args); the first registrant's config wins",
                     ownerName(owner), address, static_cast<int>(cc), argCount, static_cast<int>(slotCc), slotArgs);
        }

        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_rawSubs.push_back(RawSub{owner, address, fn, user});
        }

        LOGC(Debug, kCategory, "'%s' raw hooked 0x%08X (cc %d, %d args)", ownerName(owner), address,
             static_cast<int>(cc), argCount);

        return 1;
    }

    int32_t installRawDetour(const CubeApi* owner, uint32_t address, void* detour, void** trampoline)
    {
        if (!owner || !address || !detour || !trampoline)
            return 0;

        if (!hooks::detour::create(reinterpret_cast<void*>(static_cast<uintptr_t>(address)), detour, trampoline))
        {
            LOGC(Warn, kCategory, "'%s' raw detour 0x%08X failed to install", ownerName(owner), address);
            return 0;
        }

        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_rawDetours.push_back(RawDetour{owner, address});
        }

        LOGC(Debug, kCategory, "'%s' installed raw detour at 0x%08X", ownerName(owner), address);
        return 1;
    }

    int32_t removeRaw(const CubeApi* owner, uint32_t address)
    {
        bool freePool = false;
        bool removedDetour = false;

        int32_t removed = 0;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            for (size_t i = g_rawSubs.size(); i > 0; --i)
            {
                if (g_rawSubs[i - 1].owner == owner && g_rawSubs[i - 1].address == address)
                {
                    g_rawSubs.erase(g_rawSubs.begin() + (i - 1));
                    ++removed;
                }
            }

            freePool = (removed > 0 && !anyRawSubAt(address));
            for (size_t i = g_rawDetours.size(); i > 0; --i)
            {
                if (g_rawDetours[i - 1].owner == owner && g_rawDetours[i - 1].address == address)
                {
                    g_rawDetours.erase(g_rawDetours.begin() + (i - 1));
                    removedDetour = true;
                }
            }
        }

        if (freePool)
            rawpool::remove(address);
        if (removedDetour)
            hooks::detour::remove(reinterpret_cast<void*>(static_cast<uintptr_t>(address)));

        return (removed > 0 || removedDetour) ? 1 : 0;
    }

    void unsubscribeOwner(const CubeApi* owner)
    {
        if (!owner)
            return;

        int32_t removedPerHook[CUBE_HOOK_COUNT] = {};
        bool deactivate[CUBE_HOOK_COUNT] = {};
        std::vector<uint32_t> poolToFree;
        std::vector<uint32_t> detoursToRemove;

        size_t dropped = 0;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            for (size_t i = g_subs.size(); i > 0; --i)
            {
                if (g_subs[i - 1].owner != owner)
                    continue;

                const CubeHook hook = g_subs[i - 1].hook;
                g_subs.erase(g_subs.begin() + (i - 1));
                ++removedPerHook[hook];
                ++dropped;
            }

            for (int32_t h = 0; h < CUBE_HOOK_COUNT; ++h)
            {
                if (removedPerHook[h] == 0)
                    continue;

                g_subCount[h] -= removedPerHook[h];
                if (g_subCount[h] <= 0)
                {
                    g_subCount[h] = 0;
                    deactivate[h] = true;
                }
            }

            for (size_t i = g_rawSubs.size(); i > 0; --i)
            {
                if (g_rawSubs[i - 1].owner != owner)
                    continue;

                const uint32_t address = g_rawSubs[i - 1].address;
                g_rawSubs.erase(g_rawSubs.begin() + (i - 1));
                ++dropped;

                if (!anyRawSubAt(address))
                    poolToFree.push_back(address);
            }

            for (size_t i = g_rawDetours.size(); i > 0; --i)
            {
                if (g_rawDetours[i - 1].owner != owner)
                    continue;

                detoursToRemove.push_back(g_rawDetours[i - 1].address);
                g_rawDetours.erase(g_rawDetours.begin() + (i - 1));
                ++dropped;
            }
        }

        // Deactivate + drop the install hold for each built-in hook this owner held. A reservation
        // (attack/crit sampling) keeps the trampoline armed pass-through; releaseInstall disarms only
        // once nothing holds it.
        for (int32_t h = 0; h < CUBE_HOOK_COUNT; ++h)
        {
            if (removedPerHook[h] == 0)
                continue;

            const CubeHook hook = static_cast<CubeHook>(h);
            if (deactivate[h])
                setBuiltinActive(hook, false);
            releaseInstall(hook, removedPerHook[h]);
        }

        for (const uint32_t address : poolToFree)
            rawpool::remove(address);
        for (const uint32_t address : detoursToRemove)
            hooks::detour::remove(reinterpret_cast<void*>(static_cast<uintptr_t>(address)));

        if (dropped)
        {
            // Wait out in-flight dispatch of this mod's handlers before its DLL frees; covers
            // detours still armed for another mod (disarm/rawpool::remove drain their own).
            barrier::drain(g_dispatchInFlight, "unsubscribe");
            LOGC(Debug, kCategory, "dropped %zu hook(s) for an unloaded mod (drained)", dropped);
        }
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(g_mutex);

        if (!g_subs.empty() || !g_rawSubs.empty() || !g_rawDetours.empty())
            LOGC(Trace, kCategory, "cleared %zu built-in, %zu raw, %zu detour registration(s)", g_subs.size(), g_rawSubs.size(), g_rawDetours.size());

        g_subs.clear();
        g_rawSubs.clear();
        g_rawDetours.clear();

        for (int32_t i = 0; i < CUBE_HOOK_COUNT; ++i)
        {
            g_installRefcount[i] = 0;
            g_subCount[i] = 0;
            g_armed[i] = false;
        }
    }

    void shutdown()
    {
        std::vector<uint32_t> detours;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            for (const RawDetour& d : g_rawDetours)
                detours.push_back(d.address);
        }

        for (const uint32_t address : detours)
            hooks::detour::remove(reinterpret_cast<void*>(static_cast<uintptr_t>(address)));

        shutdownBuiltin();
        rawpool::shutdown();
        clear();
    }
}