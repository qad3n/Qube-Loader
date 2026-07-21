#include "core/iat.h"
#include "core/log.h"
#include "util/fmt.h"

namespace iat
{
    namespace
    {
        constexpr char kCategory[] = "iat";
    }

    bool writeSlot(void** slot, void* value)
    {
        DWORD old = 0;
        if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old))
        {
            LOGC(Error, kCategory, "VirtualProtect failed on slot 0x%08X", fmt::ptr(slot));
            return false;
        }

        *slot = value;
        DWORD restore = 0;
        if (!VirtualProtect(slot, sizeof(void*), old, &restore))
            LOGC(Warn, kCategory, "VirtualProtect restore failed on slot 0x%08X", fmt::ptr(slot));
        return true;
    }

    void* resolveImport(HMODULE dll, const char* dllName, const char* funcName)
    {
        if (!dll)
        {
            LOGC(Warn, kCategory, "%s!%s: dll not loaded", dllName, funcName);
            return nullptr;
        }

        void* real = reinterpret_cast<void*>(GetProcAddress(dll, funcName));

        if (!real)
            LOGC(Warn, kCategory, "%s!%s: GetProcAddress returned null", dllName, funcName);
        else
            LOGC(Debug, kCategory, "%s!%s: resolved export at 0x%08X", dllName, funcName, fmt::ptr(real));

        return real;
    }

    void* patchIatSlot(const char* dllName, const char* funcName, void* target, void* replacement, void*** outSlot, bool warnOnMiss)
    {
        HMODULE mod = GetModuleHandleA(nullptr);
        BYTE* base = reinterpret_cast<BYTE*>(mod);
        IMAGE_DOS_HEADER* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        IMAGE_NT_HEADERS32* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
        const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

        if (!dir.VirtualAddress)
        {
            LOGC(Warn, kCategory, "%s!%s: exe has no import directory", dllName, funcName);
            return nullptr;
        }

        bool dllFound = false;
        IMAGE_IMPORT_DESCRIPTOR* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress);

        for (; imp->Name; ++imp)
        {
            const char* name = reinterpret_cast<const char*>(base + imp->Name);
            if (_stricmp(name, dllName) != 0)
                continue;

            dllFound = true;
            IMAGE_THUNK_DATA32* thunk = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + imp->FirstThunk);

            for (; thunk->u1.Function; ++thunk)
            {
                void** slot = reinterpret_cast<void**>(&thunk->u1.Function);
                if (*slot != target)
                    continue;

                void* orig = *slot;

                if (!writeSlot(slot, replacement))
                {
                    LOGC(Warn, kCategory, "%s!%s: HOOK FAILED, IAT slot 0x%08X not writable",
                         dllName, funcName, fmt::ptr(slot));
                    return nullptr;
                }

                if (outSlot)
                    *outSlot = slot;

                LOGC(Debug, kCategory, "%s!%s: HOOKED, IAT slot 0x%08X orig 0x%08X -> hook 0x%08X",
                     dllName, funcName, fmt::ptr(slot), fmt::ptr(orig), fmt::ptr(replacement));

                return orig;
            }
        }

        const char* reason = dllFound ? "no IAT slot holds the target" : "dll not in exe import table";
        if (warnOnMiss)
            LOGC(Warn, kCategory, "%s!%s: HOOK FAILED, %s (target 0x%08X)", dllName, funcName, reason, fmt::ptr(target));
        else
            LOGC(Debug, kCategory, "%s!%s: not imported by the exe, optional hook skipped (%s)", dllName, funcName, reason);
        return nullptr;
    }
}