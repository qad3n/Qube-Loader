#include "core/crash.h"
#include "core/log.h"
#include "core/mem.h"
#include "core/paths.h"
#include "core/iat.h"
#include "core/exception_name.h"
#include "util/fmt.h"
#include "game/offsets.h"

#include <windows.h>
#include <psapi.h>
#include <dbghelp.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>

namespace crash
{
    namespace
    {
        constexpr char kCategory[] = "crash";
        constexpr char kDbgHelp[] = "dbghelp.dll";
        constexpr char kKernel32[] = "kernel32.dll";
        constexpr char kSetUefProc[] = "SetUnhandledExceptionFilter";
        constexpr char kMiniDumpProc[] = "MiniDumpWriteDump";
        constexpr char kMinidumpFileName[] = "cube_mod_crash.dmp";
        constexpr int kMaxModules = 512;
        constexpr int kMaxStackFrames = 24;
        constexpr int kStackScanWords = 1024;
        constexpr int kReturnAddrProbe = 5;
        constexpr int kLabelBufLen = 32;
        constexpr char kUnknownModule[] = "<unknown>";

        using PfnMiniDumpWriteDump = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

        using PfnSetUef = LPTOP_LEVEL_EXCEPTION_FILTER(WINAPI*)(LPTOP_LEVEL_EXCEPTION_FILTER);

        std::string g_dumpDir;
        LPTOP_LEVEL_EXCEPTION_FILTER g_prev = nullptr;
        std::atomic<bool> g_inFilter{false};

        // IAT guard so the game (or CRT) cannot displace our top-level filter after we install it.
        void** g_uefSlot = nullptr;
        PfnSetUef g_uefOrig = nullptr;

        struct ModuleHit
        {
            std::string name;
            uintptr_t base;
        };

        bool moduleAt(uintptr_t addr, ModuleHit& out)
        {
            HMODULE mods[kMaxModules];
            DWORD needed = 0;
            HANDLE proc = GetCurrentProcess();
            if (!EnumProcessModules(proc, mods, sizeof(mods), &needed))
                return false;

            const DWORD usable = needed < sizeof(mods) ? needed : sizeof(mods);
            const int count = static_cast<int>(usable / sizeof(HMODULE));
            for (int i = 0; i < count; ++i)
            {
                MODULEINFO mi{};
                if (!GetModuleInformation(proc, mods[i], &mi, sizeof(mi)))
                    continue;
                const uintptr_t moduleBase = reinterpret_cast<uintptr_t>(mi.lpBaseOfDll);
                if (addr < moduleBase || addr >= moduleBase + mi.SizeOfImage)
                    continue;
                char name[MAX_PATH] = {};
                if (GetModuleBaseNameA(proc, mods[i], name, sizeof(name)) == 0)
                    out.name = kUnknownModule;
                else
                    out.name = name;
                out.base = moduleBase;
                return true;
            }
            return false;
        }

        void describe(const char* label, uintptr_t addr)
        {
            ModuleHit m;
            if (!moduleAt(addr, m))
            {
                LOGC(Error, kCategory, "%s 0x%08X  <no module>", label, fmt::u32(addr));
                return;
            }
            const uintptr_t rva = addr - m.base;
            if (m.base == mem::base())
                LOGC(Error, kCategory, "%s 0x%08X  %s+0x%X  static=0x%08X (attribution.tsv)",
                     label, fmt::u32(addr), m.name.c_str(), fmt::u32(rva), fmt::u32(off::kImageBase + rva));
            else
                LOGC(Error, kCategory, "%s 0x%08X  %s+0x%X", label, fmt::u32(addr), m.name.c_str(), fmt::u32(rva));
        }

        void logRegisters(const CONTEXT* c)
        {
            LOGC(Error, kCategory, "EAX=%08X EBX=%08X ECX=%08X EDX=%08X", fmt::u32(c->Eax), fmt::u32(c->Ebx), fmt::u32(c->Ecx), fmt::u32(c->Edx));
            LOGC(Error, kCategory, "ESI=%08X EDI=%08X EBP=%08X ESP=%08X", fmt::u32(c->Esi), fmt::u32(c->Edi), fmt::u32(c->Ebp), fmt::u32(c->Esp));
            LOGC(Error, kCategory, "EIP=%08X EFLAGS=%08X", fmt::u32(c->Eip), fmt::u32(c->EFlags));
        }

        void logStack(const CONTEXT* c)
        {
            LOGC(Error, kCategory, "--- stack (return addresses into modules) ---");
            const uintptr_t* sp = reinterpret_cast<const uintptr_t*>(c->Esp);

            int shown = 0;
            for (int i = 0; i < kStackScanWords && shown < kMaxStackFrames; ++i)
            {
                if (!mem::readable(sp + i, sizeof(uintptr_t)))
                    break;

                const uintptr_t v = sp[i];
                ModuleHit m;

                if (!moduleAt(v, m))
                    continue;
                if (!mem::readable(reinterpret_cast<const void*>(v - kReturnAddrProbe), kReturnAddrProbe))
                    continue;

                char label[kLabelBufLen];
                snprintf(label, sizeof(label), "  [%02d]", shown);
                describe(label, v);
                ++shown;
            }
        }

        void writeMinidump(EXCEPTION_POINTERS* ep)
        {
            HMODULE dbg = LoadLibraryA(kDbgHelp);
            if (!dbg)
            {
                LOGC(Warn, kCategory, "dbghelp.dll unavailable, no minidump (wine: often incomplete)");
                return;
            }

            PfnMiniDumpWriteDump miniDumpWriteDump = reinterpret_cast<PfnMiniDumpWriteDump>(reinterpret_cast<void*>(GetProcAddress(dbg, kMiniDumpProc)));
            if (!miniDumpWriteDump)
            {
                FreeLibrary(dbg);
                return;
            }

            const std::string path = paths::join(g_dumpDir, kMinidumpFileName);
            HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                FreeLibrary(dbg);
                return;
            }

            MINIDUMP_EXCEPTION_INFORMATION info{};
            info.ThreadId = GetCurrentThreadId();
            info.ExceptionPointers = ep;
            info.ClientPointers = FALSE;

            const BOOL ok = miniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpWithIndirectlyReferencedMemory, &info, nullptr, nullptr);

            CloseHandle(file);
            FreeLibrary(dbg);

            if (ok)
                LOGC(Error, kCategory, "minidump -> %s", path.c_str());
            else
                LOGC(Warn, kCategory, "MiniDumpWriteDump failed (wine dbghelp is often incomplete)");
        }

        LONG WINAPI filter(EXCEPTION_POINTERS* ep)
        {
            if (g_inFilter.exchange(true))
                return EXCEPTION_CONTINUE_SEARCH;

            const EXCEPTION_RECORD* er = ep->ExceptionRecord;
            const CONTEXT* c = ep->ContextRecord;

            LOGC(Error, kCategory, "================ GAME CRASH ================");
            LOGC(Error, kCategory, "code 0x%08X (%s)", fmt::u32(er->ExceptionCode), core::exceptionName(er->ExceptionCode));

            if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
                LOGC(Error, kCategory, "%s address 0x%08X", er->ExceptionInformation[0] ? "write to" : "read from", fmt::u32(er->ExceptionInformation[1]));

            describe("fault ip @", reinterpret_cast<uintptr_t>(er->ExceptionAddress));
            logRegisters(c);
            logStack(c);
            writeMinidump(ep);

            LOGC(Error, kCategory, "===========================================");

            g_inFilter.store(false);
            if (g_prev)
                return g_prev(ep);
            return EXCEPTION_CONTINUE_SEARCH;
        }

        // Intercepts the game's/CRT's own SetUnhandledExceptionFilter so it never displaces ours. We
        // keep our filter as the OS top-level handler and record the caller's filter as our chain
        // target (our filter calls g_prev after logging). Returns the prior chained filter, matching
        // the API contract the caller expects. This is why a real crash now always reaches our dump.
        LPTOP_LEVEL_EXCEPTION_FILTER WINAPI hkSetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER next)
        {
            LPTOP_LEVEL_EXCEPTION_FILTER priorChain = g_prev;
            g_prev = next;
            LOGC(Debug, kCategory, "intercepted SetUnhandledExceptionFilter(0x%08X); kept our top-level filter", fmt::ptr(reinterpret_cast<void*>(next)));
            return priorChain;
        }

        void installUefGuard()
        {
            void* real = iat::resolveImport(GetModuleHandleA(kKernel32), kKernel32, kSetUefProc);
            if (!real)
                return; // not importable here; our filter still wins as the last installer

            g_uefOrig = reinterpret_cast<PfnSetUef>(iat::patchIatSlot(kKernel32, kSetUefProc, real,
                                                                      reinterpret_cast<void*>(&hkSetUnhandledExceptionFilter), &g_uefSlot, false));
            if (g_uefSlot)
                LOGC(Debug, kCategory, "SetUnhandledExceptionFilter IAT-guarded (the game cannot displace our crash filter)");
            else
                LOGC(Debug, kCategory, "game does not import SetUnhandledExceptionFilter; re-install guard skipped (our crash filter is still installed and active)");
        }

    }

    void install(const std::string& dumpDir)
    {
        g_dumpDir = dumpDir;
        g_prev = SetUnhandledExceptionFilter(filter);
        installUefGuard();

        LOGC(Debug, kCategory, "handler armed | dumps -> %s", dumpDir.c_str());
        LOGC(Debug, kCategory, "filter 0x%08X installed, previous 0x%08X", fmt::ptr(reinterpret_cast<void*>(&filter)), fmt::ptr(reinterpret_cast<void*>(g_prev)));
    }

    void remove()
    {
        if (g_uefSlot && g_uefOrig)
        {
            iat::writeSlot(g_uefSlot, reinterpret_cast<void*>(g_uefOrig));
            g_uefSlot = nullptr;
            g_uefOrig = nullptr;
        }
        SetUnhandledExceptionFilter(g_prev);
        g_prev = nullptr;
    }
}
