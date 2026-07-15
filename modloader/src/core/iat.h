#pragma once
// Import Address Table patching: swap an EXE import thunk so a DLL export the game calls routes
// through us. Shared by gamelog (OutputDebugString/MessageBox) and input_block (SetCursorPos).

#include <windows.h>

namespace iat
{
    // Guarded overwrite of one pointer-sized slot (VirtualProtect RW, write, restore).
    bool writeSlot(void** slot, void* value);

    // Resolve a named export from an already-loaded module. Logs + returns null on failure.
    void* resolveImport(HMODULE dll, const char* dllName, const char* funcName);

    // Patch the main EXE IAT slot for dllName!funcName holding `target`, replacing it with
    // `replacement`. Returns the original pointer (== target) + slot via outSlot, null on failure.
    void* patchIatSlot(const char* dllName, const char* funcName, void* target, void* replacement, void*** outSlot);
}
