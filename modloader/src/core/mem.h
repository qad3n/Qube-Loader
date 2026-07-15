#pragma once
// Guarded reads of live game memory; rebase() maps static dump addresses to runtime.
#include <cstddef>
#include <cstdint>

namespace mem
{
    uintptr_t base();
    uintptr_t rebase(uintptr_t staticAddr);
    bool readable(const void* p, size_t n);
    // Guarded byte copy behind a real (non-inlined) call so read<T> below cannot inline into a caller
    // with a compile-time-known bad address (spurious -Warray-bounds). Returns false, never faults.
    bool readBytes(uintptr_t addr, void* out, size_t n);

    // Guarded write: committed memory only, lifts then restores page protection if needed. mingw has
    // no SEH, so this VirtualQuery gate is the only thing between a mod and a crash. Never faults.
    bool writeRaw(uintptr_t addr, const void* src, size_t n);

    // Optional post-write callback fired on every successful writeRaw, so a higher layer can attribute
    // and detect contended writes without core/mem knowing about mods. Null (the default) = no cost.
    typedef void (*WriteObserver)(uintptr_t addr, size_t n);
    void setWriteObserver(WriteObserver observer);

    template <typename T>
    bool read(uintptr_t addr, T& out)
    {
        return readBytes(addr, &out, sizeof(T));
    }

    template <typename T>
    bool write(uintptr_t addr, const T& value)
    {
        return addr && writeRaw(addr, &value, sizeof(T));
    }
}
