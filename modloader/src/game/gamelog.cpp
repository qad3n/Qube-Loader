#include "game/gamelog.h"
#include "game/offsets.h"
#include "core/iat.h"
#include "core/log.h"
#include "core/mem.h"
#include "util/fmt.h"

#include <windows.h>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace gamelog
{
    namespace
    {
        constexpr char kCategory[] = "gamelog";
        constexpr char kGameCategory[] = "game";
        constexpr char kSqliteCategory[] = "sqlite";
        constexpr char kKernel32[] = "kernel32.dll";
        constexpr char kUser32[] = "user32.dll";
        constexpr char kOdsA[] = "OutputDebugStringA";
        constexpr char kOdsW[] = "OutputDebugStringW";
        constexpr char kMsgBoxA[] = "MessageBoxA";
        constexpr DWORD kDrainPollMs = 1;
        constexpr uint32_t kMaxSqliteMsgChars = 511; // bound the guarded copy of an untrusted sqlite message

        using PfnOdsA = void(WINAPI*)(LPCSTR);
        using PfnOdsW = void(WINAPI*)(LPCWSTR);
        using PfnMsgBoxA = int(WINAPI*)(HWND, LPCSTR, LPCSTR, UINT);
        using PfnSqliteLog = void(__cdecl*)(void*, int, const char*);

        PfnOdsA g_origA = nullptr;
        PfnOdsW g_origW = nullptr;
        PfnMsgBoxA g_origMsgBoxA = nullptr;
        void** g_slotA = nullptr;
        void** g_slotW = nullptr;
        void** g_slotMsgBoxA = nullptr;
        void** g_sqliteXLogSlot = nullptr;

        thread_local bool g_inHook = false;
        std::atomic<int> g_activeCalls{0};

        void emit(const char* text)
        {
            if (g_inHook)
                return;

            g_inHook = true;
            std::string s(text ? text : "");

            while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
                s.pop_back();

            if (!s.empty())
                LOGC(Debug, kGameCategory, "%s", s.c_str());
            g_inHook = false;
        }

        void WINAPI hookA(LPCSTR str)
        {
            g_activeCalls.fetch_add(1);
            emit(str);

            if (g_origA)
                g_origA(str);

            g_activeCalls.fetch_sub(1);
        }

        void WINAPI hookW(LPCWSTR wstr)
        {
            g_activeCalls.fetch_add(1);
            if (wstr)
            {
                const int n = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
                if (n > 0)
                {
                    std::vector<char> buf(n);
                    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, buf.data(), n, nullptr, nullptr);
                    emit(buf.data());
                }
            }

            if (g_origW)
                g_origW(wstr);
            g_activeCalls.fetch_sub(1);
        }

        int WINAPI hookMsgBoxA(HWND wnd, LPCSTR text, LPCSTR caption, UINT type)
        {
            g_activeCalls.fetch_add(1);
            LOGC(Error, kGameCategory, "MessageBox [%s] %s", caption ? caption : "", text ? text : "");

            g_activeCalls.fetch_sub(1);
            if (g_origMsgBoxA)
                return g_origMsgBoxA(wnd, text, caption, type);

            return IDOK;
        }

        void __cdecl sqliteLogHook(void*, int errCode, const char* msg)
        {
            g_activeCalls.fetch_add(1);
            // msg is untrusted: copy it one guarded byte at a time (null or cap) so a "%s"
            // print cannot run off a page into a fault (no SEH).
            char buffer[kMaxSqliteMsgChars + 1];
            uint32_t i = 0;
            if (msg)
            {
                const uintptr_t addr = reinterpret_cast<uintptr_t>(msg);
                for (; i < kMaxSqliteMsgChars; ++i)
                {
                    uint8_t ch = 0;
                    if (!mem::read(addr + i, ch) || ch == 0)
                        break;
                    buffer[i] = static_cast<char>(ch);
                }
            }

            buffer[i] = '\0';
            if (i > 0)
                LOGC(Warn, kSqliteCategory, "(%d) %s", errCode, buffer);
            g_activeCalls.fetch_sub(1);
        }

        bool installOds()
        {
            HMODULE k32 = GetModuleHandleA(kKernel32);
            void* realA = iat::resolveImport(k32, kKernel32, kOdsA);
            void* realW = iat::resolveImport(k32, kKernel32, kOdsW);

            if (realA)
                g_origA = reinterpret_cast<PfnOdsA>(iat::patchIatSlot(kKernel32, kOdsA, realA, reinterpret_cast<void*>(&hookA), &g_slotA));
            if (realW)
                g_origW = reinterpret_cast<PfnOdsW>(iat::patchIatSlot(kKernel32, kOdsW, realW, reinterpret_cast<void*>(&hookW), &g_slotW));
            return g_origA || g_origW;
        }

        bool installMsgBox()
        {
            HMODULE user32 = GetModuleHandleA(kUser32);
            void* real = iat::resolveImport(user32, kUser32, kMsgBoxA);
            if (!real)
                return false;

            g_origMsgBoxA = reinterpret_cast<PfnMsgBoxA>(iat::patchIatSlot(kUser32, kMsgBoxA, real, reinterpret_cast<void*>(&hookMsgBoxA), &g_slotMsgBoxA));
            return g_origMsgBoxA != nullptr;
        }

        bool installSqliteLog()
        {
            void** xLog = reinterpret_cast<void**>(mem::rebase(off::kSqliteXLog));
            void** logArg = reinterpret_cast<void**>(mem::rebase(off::kSqlitePLogArg));

            LOGC(Debug, kCategory, "sqlite xLog: static 0x%08X -> live 0x%08X, pLogArg static 0x%08X -> live 0x%08X", fmt::u32(off::kSqliteXLog), fmt::ptr(xLog), fmt::u32(off::kSqlitePLogArg), fmt::ptr(logArg));

            if (!mem::readable(xLog, sizeof(void*)) || !mem::readable(logArg, sizeof(void*)))
            {
                LOGC(Warn, kCategory, "sqlite xLog: HOOK FAILED, global 0x%08X not readable",
                     fmt::ptr(xLog));
                return false;
            }

            if (*xLog != nullptr)
            {
                LOGC(Warn, kCategory, "sqlite xLog: HOOK FAILED, already set to 0x%08X, leaving it",
                     fmt::ptr(*xLog));
                return false;
            }

            if (!iat::writeSlot(logArg, nullptr) || !iat::writeSlot(xLog, reinterpret_cast<void*>(&sqliteLogHook)))
            {
                LOGC(Warn, kCategory, "sqlite xLog: HOOK FAILED, global 0x%08X not writable",
                     fmt::ptr(xLog));
                return false;
            }

            g_sqliteXLogSlot = xLog;
            LOGC(Debug, kCategory, "sqlite xLog: HOOKED, callback 0x%08X written to 0x%08X", fmt::ptr(reinterpret_cast<void*>(&sqliteLogHook)), fmt::ptr(xLog));
            return true;
        }

    }

    bool install()
    {
        LOGC(Debug, kCategory, "installing capture hooks, exe base 0x%08X", fmt::u32(mem::base()));

        const bool ods = installOds();
        const bool msgBox = installMsgBox();
        const bool sqlite = installSqliteLog();

        if (!ods && !msgBox && !sqlite)
        {
            LOGC(Warn, kCategory, "no game output channel could be hooked");
            return false;
        }

        LOGC(Debug, kCategory, "capturing ods A:%d W:%d msgbox:%d sqlite:%d", g_origA != nullptr, g_origW != nullptr, msgBox, sqlite);
        return true;
    }

    void remove()
    {
        if (g_slotA && g_origA)
        {
            iat::writeSlot(g_slotA, reinterpret_cast<void*>(g_origA));
            LOGC(Debug, kCategory, "%s: restored IAT slot 0x%08X -> 0x%08X", kOdsA, fmt::ptr(g_slotA), fmt::ptr(reinterpret_cast<void*>(g_origA)));
            g_slotA = nullptr;
        }

        if (g_slotW && g_origW)
        {
            iat::writeSlot(g_slotW, reinterpret_cast<void*>(g_origW));
            LOGC(Debug, kCategory, "%s: restored IAT slot 0x%08X -> 0x%08X", kOdsW, fmt::ptr(g_slotW), fmt::ptr(reinterpret_cast<void*>(g_origW)));
            g_slotW = nullptr;
        }

        if (g_slotMsgBoxA && g_origMsgBoxA)
        {
            iat::writeSlot(g_slotMsgBoxA, reinterpret_cast<void*>(g_origMsgBoxA));
            LOGC(Debug, kCategory, "%s: restored IAT slot 0x%08X -> 0x%08X", kMsgBoxA, fmt::ptr(g_slotMsgBoxA), fmt::ptr(reinterpret_cast<void*>(g_origMsgBoxA)));
            g_slotMsgBoxA = nullptr;
        }

        if (g_sqliteXLogSlot && *g_sqliteXLogSlot == reinterpret_cast<void*>(&sqliteLogHook))
        {
            iat::writeSlot(g_sqliteXLogSlot, nullptr);
            LOGC(Debug, kCategory, "sqlite xLog: cleared 0x%08X", fmt::ptr(g_sqliteXLogSlot));
            g_sqliteXLogSlot = nullptr;
        }
        // Residual race inherent to unhooking: a thread past the slot read but not yet
        // counted is not covered; the drain narrows but cannot close it.
        while (g_activeCalls.load() > 0)
            Sleep(kDrainPollMs);
    }

}
