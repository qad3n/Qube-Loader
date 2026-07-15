#include "util/inflight.h"
#include "core/log.h"

#include <windows.h>

namespace barrier
{
    namespace
    {

        constexpr char kCategory[] = "gamehooks";
        constexpr int kSpinBudget = 4000; // tight spins before we start yielding the timeslice
        constexpr int kMaxWaitMs = 200; // absolute cap so a wedged handler cannot hang teardown

    }

    void drain(std::atomic<int>& counter, const char* what)
    {
        int spins = 0;
        int waitedMs = 0;
        while (counter.load(std::memory_order_acquire) != 0)
        {
            if (spins < kSpinBudget)
            {
                YieldProcessor();
                ++spins;
                continue;
            }
            if (waitedMs >= kMaxWaitMs)
            {
                LOGC(Warn, kCategory, "%s: drain timed out with %d handler(s) still in flight; proceeding",
                     what, counter.load(std::memory_order_acquire));
                return;
            }
            Sleep(1);
            ++waitedMs;
        }
    }
}