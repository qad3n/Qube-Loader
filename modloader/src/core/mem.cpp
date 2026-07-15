#include "core/mem.h"
#include "core/log.h"
#include "game/offsets.h"

#include <windows.h>
#include <cstdint>
#include <cstring>

namespace mem
{
    namespace
    {
        constexpr char kCategory[] = "mem";
        constexpr DWORD kProtectionMask = 0xFF;

        uintptr_t g_base = 0;
        WriteObserver g_writeObserver = nullptr;

        bool protectionReadable(DWORD protect)
        {
            switch (protect & kProtectionMask)
            {
                case PAGE_READONLY:
                case PAGE_READWRITE:
                case PAGE_WRITECOPY:
                case PAGE_EXECUTE_READ:
                case PAGE_EXECUTE_READWRITE:
                case PAGE_EXECUTE_WRITECOPY:
                    return true;
                default:
                    return false;
            }
        }

        bool protectionWritable(DWORD protect)
        {
            switch (protect & kProtectionMask)
            {
                case PAGE_READWRITE:
                case PAGE_WRITECOPY:
                case PAGE_EXECUTE_READWRITE:
                case PAGE_EXECUTE_WRITECOPY:
                    return true;
                default:
                    return false;
            }
        }

        bool regionContains(const MEMORY_BASIC_INFORMATION& mbi, const void* p, size_t n)
        {
            const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
            return n <= regionEnd - reinterpret_cast<uintptr_t>(p);
        }

        bool writable(const void* p, size_t n)
        {
            if (!p)
                return false;
            MEMORY_BASIC_INFORMATION mbi{};

            if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0)
                return false;
            if (mbi.State != MEM_COMMIT)
                return false;
            if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))
                return false;
            if (!protectionWritable(mbi.Protect))
                return false;

            return regionContains(mbi, p, n);
        }

        void notifyWrite(uintptr_t addr, size_t n)
        {
            if (g_writeObserver)
                g_writeObserver(addr, n);
        }

    }

    void setWriteObserver(WriteObserver observer)
    {
        g_writeObserver = observer;
    }

    uintptr_t base()
    {
        if (!g_base)
            g_base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));

        return g_base;
    }

    uintptr_t rebase(uintptr_t staticAddr)
    {
        return staticAddr - off::kImageBase + base();
    }

    bool readable(const void* p, size_t n)
    {
        if (!p)
            return false;

        MEMORY_BASIC_INFORMATION mbi{};

        if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0)
            return false;
        if (mbi.State != MEM_COMMIT)
            return false;
        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))
            return false;
        if (!protectionReadable(mbi.Protect))
            return false;

        return regionContains(mbi, p, n);
    }

    bool readBytes(uintptr_t addr, void* out, size_t n)
    {
        if (!addr || !out || n == 0 || !readable(reinterpret_cast<const void*>(addr), n))
            return false;

        std::memcpy(out, reinterpret_cast<const void*>(addr), n);
        return true;
    }

    bool writeRaw(uintptr_t addr, const void* src, size_t n)
    {
        if (!addr || !src || n == 0)
            return false;

        void* dst = reinterpret_cast<void*>(addr);
        if (writable(dst, n))
        {
            std::memcpy(dst, src, n);
            notifyWrite(addr, n);
            return true;
        }

        if (!readable(dst, n))
            return false;

        DWORD oldProtect = 0;
        if (!VirtualProtect(dst, n, PAGE_EXECUTE_READWRITE, &oldProtect))
            return false;

        std::memcpy(dst, src, n);
        DWORD restored = 0;
        if (!VirtualProtect(dst, n, oldProtect, &restored))
            LOGC(Warn, kCategory, "failed to restore page protection at 0x%08X", static_cast<unsigned>(addr));

        notifyWrite(addr, n);
        return true;
    }
}
