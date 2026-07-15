#pragma once
// Teardown barrier for game-thread hook dispatch: a detour bumps an in-flight counter around the
// handler chain; teardown flips the arm gate to pass-through, then drains to zero before freeing the
// trampoline / unloading the DLL. Bounded, so a wedged handler cannot hang the eject path forever.
#include <atomic>

namespace barrier
{

    // RAII in-flight marker.
    class InFlight
    {
    public:
        explicit InFlight(std::atomic<int>& counter) : m_counter(counter)
        {
            m_counter.fetch_add(1, std::memory_order_acq_rel);
        }

        ~InFlight()
        {
            m_counter.fetch_sub(1, std::memory_order_acq_rel);
        }

        InFlight(const InFlight&) = delete;
        InFlight& operator=(const InFlight&) = delete;

    private:
        std::atomic<int>& m_counter;
    };

    // Spin, yield, then bounded-sleep until counter hits zero. Caller MUST have already flipped the
    // arm gate so no new dispatch can bump it. On timeout logs and proceeds (liveness over a full drain).
    void drain(std::atomic<int>& counter, const char* what);
}