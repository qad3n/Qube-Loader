#pragma once
// Formatting helpers for logging addresses/words with printf %X.
#include <cstdint>

namespace fmt
{
    inline unsigned u32(uintptr_t v)
    {
        return static_cast<unsigned>(v);
    }

    inline unsigned ptr(const void* p)
    {
        return static_cast<unsigned>(reinterpret_cast<uintptr_t>(p));
    }
}