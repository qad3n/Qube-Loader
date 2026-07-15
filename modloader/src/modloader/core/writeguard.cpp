#include "modloader/core/writeguard.h"
#include "modloader/core/conflict.h"
#include "modloader/core/owner_name.h"
#include "modloader/game/gameevents.h"
#include "core/mem.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace modloader::writeguard
{
    namespace
    {
        constexpr uint32_t kWarnCooldownFrames = 300; // re-warn a given pair+address at most this often

        thread_local const CubeApi* g_writer = nullptr;

        struct Record
        {
            const CubeApi* owner;
            uint32_t frame;
        };

        std::mutex g_mutex;
        std::unordered_map<uintptr_t, Record> g_lastWrite;

        uint64_t conflictSignature(const CubeApi* a, const CubeApi* b, uintptr_t addr)
        {
            const uintptr_t lo = reinterpret_cast<uintptr_t>(a < b ? a : b);
            const uintptr_t hi = reinterpret_cast<uintptr_t>(a < b ? b : a);
            return static_cast<uint64_t>(lo) ^ (static_cast<uint64_t>(hi) << 1) ^ (static_cast<uint64_t>(addr) << 2);
        }

        void observe(uintptr_t addr, size_t n)
        {
            const CubeApi* writer = g_writer;
            if (!writer)
                return; // internal loader/diag write, not attributable to a mod

            const uint32_t frame = gameevents::currentFrame();
            const CubeApi* contender = nullptr;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                std::unordered_map<uintptr_t, Record>::iterator it = g_lastWrite.find(addr);
                if (it != g_lastWrite.end() && it->second.owner && it->second.owner != writer)
                    contender = it->second.owner;

                g_lastWrite[addr] = Record{writer, frame};
            }

            if (contender && conflict::throttle(conflictSignature(writer, contender, addr), frame, kWarnCooldownFrames))
                conflict::warn("'%s' and '%s' both write 0x%08X (%zu bytes); last-writer-wins, the game may misbehave",
                               ownerName(writer), ownerName(contender), static_cast<unsigned>(addr), n);
        }
    }

    Scope::Scope(const CubeApi* owner)
        : m_previous(g_writer)
    {
        g_writer = owner;
    }

    Scope::~Scope()
    {
        g_writer = m_previous;
    }

    void install()
    {
        mem::setWriteObserver(&observe);
    }

    void remove()
    {
        mem::setWriteObserver(nullptr);
        std::lock_guard<std::mutex> lock(g_mutex);
        g_lastWrite.clear();
    }
}
