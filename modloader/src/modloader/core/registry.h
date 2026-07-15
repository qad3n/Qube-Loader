#pragma once
// Owner-tagged subscription registry behind the event bus: a sub drops by token (one) or owner (all).
// Thread-safe: snapshotInto() copies matching subs under the lock so dispatch runs without holding it.
#include "cube_sdk.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace modloader
{
    // Sub must expose public members: const CubeApi* owner; uint32_t token;
    template <typename Sub>
    class OwnerRegistry
    {
    public:
        // Adds s (stamping a fresh nonzero token) and returns that token.
        uint32_t add(Sub s)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const uint32_t token = m_nextToken++;
            s.token = token;
            m_subs.push_back(s);
            return token;
        }

        // Copies every sub matching pred into out (cleared, capacity reused) under the lock, for
        // lock-free dispatch afterwards. Pass a reusable buffer to keep the hot path allocation-free.
        template <typename Pred>
        void snapshotInto(std::vector<Sub>& out, Pred pred) const
        {
            out.clear();
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const Sub& s : m_subs)
            {
                if (pred(s))
                    out.push_back(s);
            }
        }

        // Removes every sub matching pred (reverse iteration so erase stays valid); returns the count.
        template <typename Pred>
        std::size_t removeIf(Pred pred)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            std::size_t removed = 0;
            for (std::size_t i = m_subs.size(); i > 0; --i)
            {
                if (!pred(m_subs[i - 1]))
                    continue;
                m_subs.erase(m_subs.begin() + static_cast<std::ptrdiff_t>(i - 1));
                ++removed;
            }
            return removed;
        }

        std::size_t removeToken(uint32_t token)
        {
            return removeIf([token](const Sub& s) { return s.token == token; });
        }

        std::size_t removeOwner(const CubeApi* owner)
        {
            return removeIf([owner](const Sub& s) { return s.owner == owner; });
        }

        template <typename Fn>
        void forEach(Fn fn) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const Sub& s : m_subs)
                fn(s);
        }

        std::size_t size() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_subs.size();
        }

        void clear()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_subs.clear();
        }

    private:
        mutable std::mutex m_mutex;
        std::vector<Sub> m_subs;
        uint32_t m_nextToken = 1;
    };
}