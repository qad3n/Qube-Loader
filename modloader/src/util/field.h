#pragma once
// Guarded reads/writes of a single struct field at base+offset. A 0 offset
// means the field is unlocated and is skipped, leaving the caller's default.

#include "core/mem.h"
#include <cstdint>
#include <cstring>

namespace field
{
    inline void i32(uintptr_t base, uintptr_t fieldOff, int32_t& out)
    {
        uint32_t value = 0;
        if (fieldOff && mem::read(base + fieldOff, value))
            out = static_cast<int32_t>(value);
    }

    inline void u32(uintptr_t base, uintptr_t fieldOff, uint32_t& out)
    {
        uint32_t value = 0;
        if (fieldOff && mem::read(base + fieldOff, value))
            out = value;
    }

    inline void byteI32(uintptr_t base, uintptr_t fieldOff, int32_t& out)
    {
        uint8_t value = 0;
        if (fieldOff && mem::read(base + fieldOff, value))
            out = static_cast<int32_t>(value);
    }

    inline void f32(uintptr_t base, uintptr_t fieldOff, float& out)
    {
        uint32_t value = 0;
        if (fieldOff && mem::read(base + fieldOff, value))
            std::memcpy(&out, &value, sizeof(out));
    }

    // Reads a signed 16.16-style int64 fixed-point field and scales to float.
    inline void fixed64(uintptr_t base, uintptr_t fieldOff, double scale, float& out)
    {
        int64_t raw = 0;
        if (fieldOff && mem::read(base + fieldOff, raw))
            out = static_cast<float>(static_cast<double>(raw) / scale);
    }

    // Reads three contiguous int64 fixed-point fields (a position triple) to floats.
    inline void vec3Fixed64(uintptr_t base, uintptr_t baseOff, double scale, float& x, float& y, float& z)
    {
        fixed64(base, baseOff, scale, x);
        fixed64(base, baseOff + sizeof(int64_t), scale, y);
        fixed64(base, baseOff + sizeof(int64_t) * 2, scale, z);
    }

    // Reads a 32-bit MSVC std::vector header (begin@+0, end@+4): yields the begin pointer + count
    // capped at maxElems. Shared by every std::vector walk (inventory/structures/corruption scan).
    inline bool vectorSpan(uintptr_t vecAddr, uintptr_t stride, int32_t maxElems, uint32_t& begin, int32_t& count)
    {
        uint32_t end = 0;
        begin = 0;
        count = 0;

        if (stride == 0 || !mem::read(vecAddr, begin) || !mem::read(vecAddr + sizeof(uint32_t), end) || !begin || end <= begin)
            return false;

        const uint32_t span = (end - begin) / static_cast<uint32_t>(stride);
        count = span < static_cast<uint32_t>(maxElems) ? static_cast<int32_t>(span) : maxElems;
        return true;
    }

    // Guarded writes of a single struct field. A 0 offset means unlocated: the
    // write is skipped (returns false).
    inline bool setI32(uintptr_t base, uintptr_t fieldOff, int32_t value)
    {
        return fieldOff && mem::write(base + fieldOff, value);
    }

    inline bool setF32(uintptr_t base, uintptr_t fieldOff, float value)
    {
        return fieldOff && mem::write(base + fieldOff, value);
    }

    inline bool setU8(uintptr_t base, uintptr_t fieldOff, uint8_t value)
    {
        return fieldOff && mem::write(base + fieldOff, value);
    }

    inline bool setFixed64(uintptr_t base, uintptr_t fieldOff, double scale, float value)
    {
        const int64_t raw = static_cast<int64_t>(static_cast<double>(value) * scale);
        return fieldOff && mem::write(base + fieldOff, raw);
    }

    // Reads a narrow or wide null-terminated string into a char buffer of
    // capacity maxChars+1. Returns true if at least one char was read.
    inline bool cstr(uintptr_t base, uintptr_t fieldOff, bool wide, uint32_t maxChars, char* out)
    {
        out[0] = '\0';
        if (!fieldOff)
            return false;

        constexpr uint32_t kMaxCstrBytes = 256;
        const uintptr_t stride = wide ? 2 : 1;
        if (maxChars > kMaxCstrBytes / stride)
            maxChars = static_cast<uint32_t>(kMaxCstrBytes / stride);
        const uintptr_t addr = base + fieldOff;

        // One guarded copy of the whole span (readBytes VirtualQuery-gates once), then scan the buffer,
        // instead of a per-byte guarded read that re-validates the same page maxChars times.
        uint8_t buf[kMaxCstrBytes];
        if (!mem::readBytes(addr, buf, maxChars * stride))
            return false;

        uint32_t i = 0;
        for (; i < maxChars; ++i)
        {
            const uint8_t ch = buf[i * stride];
            if (ch == 0)
                break;
            out[i] = static_cast<char>(ch);
        }

        out[i] = '\0';
        return i > 0;
    }
}