#include "modloader/core/internal.h"
#include "modloader/core/conflict.h"
#include "core/log.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

// Dependency resolution: after scan() and before CUBE_EVENT_READY, unload any mod whose hard
// dependency is missing or below its declared minVersion (cascading to that mod's dependents), warn
// on missing soft deps, then topological-rank the surviving mods so a dependency dispatches before its
// dependents at equal priority (context.dispatchOrder).
namespace modloader
{
    namespace
    {
        bool isNumeric(const std::string& s)
        {
            if (s.empty())
                return false;
            for (char c : s)
            {
                if (c < '0' || c > '9')
                    return false;
            }
            return true;
        }

        std::vector<std::string> splitVersion(const std::string& v)
        {
            std::vector<std::string> out;
            std::string cur;
            for (char c : v)
            {
                if (c == '.')
                {
                    out.push_back(cur);
                    cur.clear();
                }
                else
                    cur.push_back(c);
            }
            out.push_back(cur);
            return out;
        }

        // Dotted compare: numeric per component where both parse, else lexical; missing component = "0".
        // Returns <0 if lhs precedes rhs, 0 if equal, >0 otherwise.
        int compareVersions(const std::string& lhs, const std::string& rhs)
        {
            const std::vector<std::string> a = splitVersion(lhs);
            const std::vector<std::string> b = splitVersion(rhs);
            const std::size_t n = a.size() > b.size() ? a.size() : b.size();
            for (std::size_t i = 0; i < n; ++i)
            {
                const std::string ca = i < a.size() ? a[i] : "0";
                const std::string cb = i < b.size() ? b[i] : "0";
                if (isNumeric(ca) && isNumeric(cb))
                {
                    const long va = std::strtol(ca.c_str(), nullptr, 10);
                    const long vb = std::strtol(cb.c_str(), nullptr, 10);
                    if (va != vb)
                        return va < vb ? -1 : 1;
                }
                else
                {
                    const int c = ca.compare(cb);
                    if (c != 0)
                        return c < 0 ? -1 : 1;
                }
            }
            return 0;
        }

        std::map<std::string, LoadedMod*> indexById()
        {
            std::map<std::string, LoadedMod*> byId;
            for (const std::unique_ptr<LoadedMod>& mod : loadedMods())
                byId[mod->context.id] = mod.get();
            return byId;
        }

        // Find the first unmet HARD dep of mod (given the current provider map). Returns the mod itself
        // (with reason logged) when one is unmet, or nullptr when all hard deps are satisfied.
        LoadedMod* firstUnmetHardDep(LoadedMod* mod, const std::map<std::string, LoadedMod*>& byId)
        {
            for (const Dep& dep : mod->deps)
            {
                if (!dep.hard)
                    continue;
                const std::map<std::string, LoadedMod*>::const_iterator it = byId.find(dep.id);
                if (it == byId.end())
                {
                    conflict::error("mod '%s' requires '%s' which is not loaded; unloading '%s'",
                                    mod->context.id.c_str(), dep.id.c_str(), mod->context.id.c_str());
                    return mod;
                }
                if (!dep.minVersion.empty() && compareVersions(it->second->version, dep.minVersion) < 0)
                {
                    conflict::error("mod '%s' requires '%s' >= v%s but v%s is loaded; unloading '%s'",
                                    mod->context.id.c_str(), dep.id.c_str(), dep.minVersion.c_str(),
                                    it->second->version.c_str(), mod->context.id.c_str());
                    return mod;
                }
            }
            return nullptr;
        }

        // Unload every mod with an unmet hard dep, restarting after each unload so a mod cascades out
        // once a dependency it needed has just been removed.
        void unloadUnsatisfied()
        {
            bool progress = true;
            while (progress)
            {
                progress = false;
                const std::map<std::string, LoadedMod*> byId = indexById();
                for (const std::unique_ptr<LoadedMod>& mod : loadedMods())
                {
                    LoadedMod* offender = firstUnmetHardDep(mod.get(), byId);
                    if (offender)
                    {
                        unloadOne(offender);
                        progress = true;
                        break; // loadedMods() mutated; rebuild the map and rescan
                    }
                }
            }
        }

        void warnMissingSoftDeps(const std::map<std::string, LoadedMod*>& byId)
        {
            for (const std::unique_ptr<LoadedMod>& mod : loadedMods())
            {
                for (const Dep& dep : mod->deps)
                {
                    if (dep.hard)
                        continue;
                    if (byId.find(dep.id) == byId.end())
                        conflict::warn("mod '%s' optionally uses '%s' which is not loaded; continuing without it",
                                       mod->context.id.c_str(), dep.id.c_str());
                }
            }
        }

        // Kahn topological order over the survivors (a dependency ranks before its dependents), writing
        // the resulting rank to each mod's context.dispatchOrder. Any mod left in a cycle keeps load
        // order after the acyclic prefix, with a warning.
        void assignDispatchOrder(const std::map<std::string, LoadedMod*>& byId)
        {
            std::vector<std::unique_ptr<LoadedMod>>& mods = loadedMods();
            const std::size_t count = mods.size();

            // inDegree = how many present prerequisites each mod still waits on; dependents = reverse edges.
            std::map<LoadedMod*, int> inDegree;
            std::map<LoadedMod*, std::vector<LoadedMod*>> dependents;
            for (const std::unique_ptr<LoadedMod>& mod : mods)
            {
                int prereqs = 0;
                for (const Dep& dep : mod->deps)
                {
                    const std::map<std::string, LoadedMod*>::const_iterator it = byId.find(dep.id);
                    if (it == byId.end() || it->second == mod.get())
                        continue;
                    ++prereqs;
                    dependents[it->second].push_back(mod.get());
                }
                inDegree[mod.get()] = prereqs;
            }

            // Seed with zero-prerequisite mods in load order, so ties keep load order.
            std::vector<LoadedMod*> queue;
            for (const std::unique_ptr<LoadedMod>& mod : mods)
            {
                if (inDegree[mod.get()] == 0)
                    queue.push_back(mod.get());
            }

            int32_t rank = 0;
            std::size_t head = 0;
            std::map<LoadedMod*, bool> ranked;
            while (head < queue.size())
            {
                LoadedMod* mod = queue[head++];
                mod->context.dispatchOrder = rank++;
                ranked[mod] = true;
                for (LoadedMod* dependent : dependents[mod])
                {
                    if (--inDegree[dependent] == 0)
                        queue.push_back(dependent);
                }
            }

            if (ranked.size() != count)
            {
                conflict::warn("dependency cycle among %zu mod(s); dispatch order falls back to load order for them",
                               count - ranked.size());
                for (const std::unique_ptr<LoadedMod>& mod : mods)
                {
                    if (!ranked[mod.get()])
                        mod->context.dispatchOrder = rank++;
                }
            }
        }
    }

    void resolveDependencies()
    {
        if (loadedMods().empty())
            return;

        const std::size_t before = loadedMods().size();
        unloadUnsatisfied();
        const std::size_t after = loadedMods().size();

        const std::map<std::string, LoadedMod*> byId = indexById();
        warnMissingSoftDeps(byId);
        assignDispatchOrder(byId);

        if (before != after)
            LOGC(Info, kCategory, "dependency resolution: %zu mod(s) unloaded, %zu remain", before - after, after);
        else
            LOGC(Debug, kCategory, "dependency resolution: all %zu mod(s) satisfied", after);
    }
}
