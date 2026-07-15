#pragma once
// Storage: the mod-facing save-data facade. Reads/writes this mod's own binary blobs under
// <dllDir>/data/<stem>/ (the loader keys it by the mod's DLL stem). Distinct from Config (user-editable
// text settings); this is mod-owned, binary-safe state - counters, discovered progress. Get with
// mod.storage(); an optional setScope() namespaces blobs per world/character.

#include "cube/common.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace cube
{
    class Storage
    {
    public:
        explicit Storage(const CubeApi* api = nullptr) : m_api(api) {}

        // Namespace subsequent calls under a scope (e.g. world seed / character). "" restores the root.
        void setScope(const char* scope) const
        {
            if (m_api)
                m_api->storage.setScope(m_api, scope);
        }

        bool has(const char* key) const
        {
            return m_api && m_api->storage.has(m_api, key) != 0;
        }

        bool remove(const char* key) const
        {
            return m_api && m_api->storage.remove(m_api, key) != 0;
        }

        // Stored blob size in bytes (0 if absent), without reading it.
        int size(const char* key) const
        {
            return m_api ? m_api->storage.get(m_api, key, nullptr, 0) : 0;
        }

        // The whole blob at key (empty if absent).
        std::vector<uint8_t> get(const char* key) const
        {
            if (!m_api)
                return {};
            const int stored = m_api->storage.get(m_api, key, nullptr, 0);
            if (stored <= 0)
                return {};
            std::vector<uint8_t> buffer(static_cast<size_t>(stored));
            const int read = m_api->storage.get(m_api, key, buffer.data(), stored);
            buffer.resize(read > 0 ? static_cast<size_t>(read) : 0);
            return buffer;
        }

        bool put(const char* key, const void* data, int size) const
        {
            return m_api && m_api->storage.put(m_api, key, data, size) != 0;
        }

        // Persist/read a trivially-copyable value (a counter, a small POD). getValue returns fallback
        // unless a blob of exactly sizeof(T) exists.
        template <typename T>
        bool putValue(const char* key, const T& value) const
        {
            return put(key, &value, static_cast<int>(sizeof(T)));
        }

        template <typename T>
        T getValue(const char* key, T fallback = T{}) const
        {
            T value = fallback;
            if (m_api && m_api->storage.get(m_api, key, &value, static_cast<int>(sizeof(T))) == static_cast<int>(sizeof(T)))
                return value;
            return fallback;
        }

        bool putString(const char* key, const std::string& value) const
        {
            return put(key, value.data(), static_cast<int>(value.size()));
        }

        std::string getString(const char* key) const
        {
            const std::vector<uint8_t> blob = get(key);
            return std::string(blob.begin(), blob.end());
        }

    private:
        const CubeApi* m_api;
    };

}
