#include "core/faultguard.h"
#include "core/log.h"
#include "core/exception_name.h"
#include "modloader/core/owner_name.h"
#include "modloader/core/modregistry.h"

#include <windows.h>
#include <atomic>
#include <csetjmp>
#include <mutex>
#include <vector>

// longjmp out of a vectored handler is only safe with DWARF-2 EH (static unwind tables). Under SjLj
// EH it would corrupt the thread's exception-registration chain, so refuse to build that way.
#ifdef __USING_SJLJ_EXCEPTIONS__
#error "faultguard requires the DWARF-2 EH mingw toolchain, not SjLj (see comment above)."
#endif

// Only faults inside a live guard frame (g_top set) are isolated; a real crash falls through to
// crash.cpp. longjmp skips C++ destructors up to the setjmp boundary (ok, the mod is disabled).
// Heap-corruption faults are undetectable.
namespace faultguard
{
    namespace
    {

        constexpr char kCategory[] = "faultguard";
        constexpr int kFirstHandler = 1; // AddVectoredExceptionHandler: place ahead of others
        using modloader::ownerName;
        using modloader::ownerStem;

        struct GuardFrame
        {
            jmp_buf jb;
            const CubeApi* owner;
            GuardFrame* prev;
            volatile uint32_t faultCode;
            volatile uint32_t faultAddr;
        };

        // Innermost active guard frame on the calling thread (nullptr = not inside a mod callback).
        thread_local GuardFrame* g_top = nullptr;

        std::atomic<bool> g_enabled{false};
        PVOID g_handle = nullptr;

        // Quarantine set. g_count gives the hot dispatch path a lock-free fast exit when empty.
        std::mutex g_quarMutex;
        std::vector<const CubeApi*> g_quarantined;
        std::atomic<int> g_count{0};

        // Faults we recover from. Deliberately excludes STACK_OVERFLOW (the guard page is gone, so
        // resuming is unsafe) and the C++ EH code 0xE06D7363 (let the try/catch handle those).
        bool isIsolatableFault(DWORD code)
        {
            switch (code)
            {
                case EXCEPTION_ACCESS_VIOLATION:
                case EXCEPTION_IN_PAGE_ERROR:
                case EXCEPTION_ILLEGAL_INSTRUCTION:
                case EXCEPTION_PRIV_INSTRUCTION:
                case EXCEPTION_INT_DIVIDE_BY_ZERO:
                case EXCEPTION_FLT_DIVIDE_BY_ZERO:
                case EXCEPTION_FLT_INVALID_OPERATION:
                case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
                    return true;
                default:
                    return false;
            }
        }

        void addQuarantine(const CubeApi* owner)
        {
            if (!owner)
                return;

            std::lock_guard<std::mutex> lock(g_quarMutex);
            for (const CubeApi* existing : g_quarantined)
            {
                if (existing == owner)
                    return;
            }

            g_quarantined.push_back(owner);
            g_count.store(static_cast<int>(g_quarantined.size()), std::memory_order_release);
        }

        // Runs on the faulting thread after longjmp; no dispatch lock held here, so recording the
        // quarantine cannot deadlock. Unsubscribe/free is deferred to teardown; the mod is skipped meanwhile.
        void onFault(const CubeApi* owner, uint32_t code, uint32_t addr, const char* what)
        {
            if (!owner)
            {
                // Loader's own game-thread detour (no mod to disable): recover and log so it is never a
                // silent crash; the caller applies its safe fallback (e.g. the vanilla result).
                LOGC(Error, kCategory, "loader detour '%s' faulted (%s @0x%08X) - recovered; the game continues",
                     what ? what : "?", core::exceptionName(code), static_cast<unsigned>(addr));
                return;
            }

            addQuarantine(owner);
            LOGC(Error, kCategory, "mod '%s' faulted (%s @0x%08X) - disabling it; the game continues",
                 ownerName(owner), core::exceptionName(code), static_cast<unsigned>(addr));
            // Persist a strike so a mod that faults every launch is auto-disabled instead of crash-looping.
            modloader::modregistry::recordFault(ownerStem(owner));
        }

        LONG CALLBACK vectoredHandler(EXCEPTION_POINTERS* ep)
        {
            const DWORD code = ep->ExceptionRecord->ExceptionCode;
            if (!isIsolatableFault(code))
                return EXCEPTION_CONTINUE_SEARCH;

            GuardFrame* frame = g_top;
            if (!frame)
                return EXCEPTION_CONTINUE_SEARCH; // not inside a mod callback: real crash -> crash.cpp

            frame->faultCode = static_cast<uint32_t>(code);
            frame->faultAddr = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(ep->ExceptionRecord->ExceptionAddress));

            longjmp(frame->jb, 1); // unwind to the runGuarded boundary (does not return)
        }

    }

    void install(bool enable)
    {
        g_enabled.store(enable, std::memory_order_release);
        if (!enable)
        {
            LOGC(Debug, kCategory, "fault isolation disabled by config (a mod fault will crash the game)");
            return;
        }

        g_handle = AddVectoredExceptionHandler(kFirstHandler, &vectoredHandler);
        if (!g_handle)
        {
            g_enabled.store(false, std::memory_order_release);
            LOGC(Warn, kCategory, "AddVectoredExceptionHandler failed; fault isolation off");
            return;
        }

        LOGC(Debug, kCategory, "fault isolation armed (vectored handler first in chain)");
    }

    void remove()
    {
        if (g_handle)
        {
            RemoveVectoredExceptionHandler(g_handle);
            g_handle = nullptr;
        }

        g_enabled.store(false, std::memory_order_release);

        std::lock_guard<std::mutex> lock(g_quarMutex);

        g_quarantined.clear();
        g_count.store(0, std::memory_order_release);
    }

    bool enabled()
    {
        return g_enabled.load(std::memory_order_acquire);
    }

    bool runGuarded(const char* what, const CubeApi* owner, GuardThunk fn, void* ctx)
    {
        GuardFrame frame;
        frame.owner = owner;
        frame.prev = g_top;
        frame.faultCode = 0;
        frame.faultAddr = 0;
        g_top = &frame;

        if (setjmp(frame.jb) == 0)
        {
            bool ok = true;
            try
            {
                fn(ctx);
            }
            catch (const std::exception& e)
            {
                LOGC(Error, kCategory, "%s failed: %s", what, e.what());
                ok = false;
            }
            catch (...)
            {
                LOGC(Error, kCategory, "%s failed: unknown exception", what);
                ok = false;
            }
            g_top = frame.prev;
            return ok;
        }

        // Recovered from a CPU fault via longjmp from vectoredHandler. Pop the frame BEFORE handling
        // so that a (very unlikely) fault inside onFault escalates instead of looping on this frame.
        g_top = frame.prev;
        onFault(frame.owner, frame.faultCode, frame.faultAddr, what);
        return false;
    }

    bool isQuarantined(const CubeApi* owner)
    {
        if (!owner || g_count.load(std::memory_order_acquire) == 0)
            return false;

        std::lock_guard<std::mutex> lock(g_quarMutex);
        for (const CubeApi* existing : g_quarantined)
        {
            if (existing == owner)
                return true;
        }
        return false;
    }
}
