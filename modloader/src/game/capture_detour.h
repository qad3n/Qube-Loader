#pragma once
// Shared scaffolding for a "capture on game thread, poll on render thread" detour over a no arg
// __thiscall GameController function (mingw: ABI identical to __fastcall). The detour runs the
// original, then (while armed) captures a Record on the game thread and publishes it through a seq
// fence; the render thread reads the latest via readLast()/poll(). remove() flips to pass through,
// disables the hook, and drains in flight calls before MinHook frees the trampoline (no SEH on mingw).

#include "hooks/detour.h"
#include "core/mem.h"
#include "core/log.h"
#include "util/guard.h"
#include "util/inflight.h"

#include <atomic>
#include <cstdint>

namespace game
{
    // A no arg __thiscall is ABI identical to __fastcall(self, edx) on mingw.
    typedef void(__fastcall* NoArgThiscallFn)(void* self, void* edx);

    template <typename Record>
    class CaptureDetour
    {
    public:
        CaptureDetour(const char* category, uintptr_t targetFn, const char* captureLabel,
                      const char* installMsg, const char* failMsg)
            : m_category(category), m_targetFn(targetFn), m_captureLabel(captureLabel),
              m_installMsg(installMsg), m_failMsg(failMsg)
        {
        }

        bool install(void* detour)
        {
            if (m_installed)
                return true;

            if (!hooks::detour::create(target(), detour, reinterpret_cast<void**>(&m_original)))
            {
                LOGC(Warn, m_category, "%s", m_failMsg);
                return false;
            }

            m_active.store(true, std::memory_order_release);
            m_installed = true;
            LOGC(Debug, m_category, "%s", m_installMsg);
            return true;
        }

        void remove()
        {
            if (!m_installed)
                return;

            m_active.store(false, std::memory_order_release);
            void* const hooked = target();
            hooks::detour::disable(hooked);
            barrier::drain(m_inFlight, m_category);
            hooks::detour::release(hooked);

            m_original = nullptr;
            m_installed = false;
        }

        // Called by the site's __fastcall detour: runs the original, then (while armed) invokes
        // capture(gc, record); publishes the record under the seq fence if capture returns true.
        template <typename Capture>
        void dispatch(void* self, void* edx, Capture capture)
        {
            barrier::InFlight inflight(m_inFlight);
            if (m_original)
                m_original(self, edx); // run the real update first so it commits its state

            if (!m_active.load(std::memory_order_acquire))
                return; // tearing down: pass through without touching capture state

            // Isolate a CPU fault in our capture body (the game's own update already ran above). On a
            // fault the game thread recovers and simply skips this capture rather than crashing.
            guard::tryRunLoader(m_captureLabel,
                                [&]()
                                {
                                    Record record = {};
                                    if (capture(reinterpret_cast<uintptr_t>(self), record))
                                        publish(record);
                                });
        }

        // Latest published record (render thread). False until the first capture.
        bool readLast(Record& out)
        {
            uint32_t seq = 0;
            return snapshot(out, seq);
        }

        // Edge form: true only when a new record was published since the last poll.
        bool poll(Record& out)
        {
            if (m_seq.load(std::memory_order_acquire) == m_pollSeq)
                return false;

            // Track the seq actually snapshotted (always even), so a read that raced a publish
            // cannot latch an odd value and re-emit the same record next frame.
            uint32_t seq = 0;
            if (!snapshot(out, seq))
                return false;

            m_pollSeq = seq;
            return true;
        }

        uint32_t count() const { return m_count.load(std::memory_order_relaxed); }

    private:
        void* target() const { return reinterpret_cast<void*>(mem::rebase(m_targetFn)); }

        // Seqlock: odd while writing. A reader that sees an odd count, or a different one either side
        // of its copy, retries rather than handing a mod a half old half new Record.
        void publish(const Record& record)
        {
            m_seq.fetch_add(1, std::memory_order_acq_rel);
            m_record = record;
            m_count.fetch_add(1, std::memory_order_relaxed);
            m_seq.fetch_add(1, std::memory_order_release);
        }

        bool snapshot(Record& out, uint32_t& seqOut)
        {
            if (m_count.load(std::memory_order_relaxed) == 0)
                return false;

            constexpr int kSnapshotAttempts = 8;
            for (int attempt = 0; attempt < kSnapshotAttempts; ++attempt)
            {
                const uint32_t before = m_seq.load(std::memory_order_acquire);
                if (before % 2 != 0)
                    continue;

                out = m_record;
                if (m_seq.load(std::memory_order_acquire) == before)
                {
                    seqOut = before;
                    return true;
                }
            }
            return false;
        }

        const char* m_category;
        uintptr_t m_targetFn;
        const char* m_captureLabel;
        const char* m_installMsg;
        const char* m_failMsg;

        NoArgThiscallFn m_original = nullptr;
        bool m_installed = false;
        std::atomic<bool> m_active{false};
        std::atomic<int> m_inFlight{0};

        std::atomic<uint32_t> m_seq{0};
        std::atomic<uint32_t> m_count{0};
        Record m_record = {};
        uint32_t m_pollSeq = 0; // render thread only
    };
}
