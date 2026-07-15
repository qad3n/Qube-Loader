#include "features/memory_probe.h"

#include "cube_mod.hpp"

#include <cstdint>
#include <cstdlib>

namespace exmod
{
    namespace
    {
        constexpr int kHexRadix = 16;
        constexpr unsigned kReadSize = sizeof(uint32_t);
    }

    MemoryProbe& memoryProbe()
    {
        static MemoryProbe g_memoryProbe;
        return g_memoryProbe;
    }

    unsigned MemoryProbe::staticAddress() const
    {
        return static_cast<unsigned>(std::strtoul(m_address, nullptr, kHexRadix));
    }

    unsigned MemoryProbe::runtimeAddress() const
    {
        return cube::mod().rebase(staticAddress());
    }

    bool MemoryProbe::readable() const
    {
        return cube::mod().readable(runtimeAddress(), kReadSize);
    }

    unsigned MemoryProbe::readU32() const
    {
        return cube::mod().read<uint32_t>(runtimeAddress());
    }

    bool MemoryProbe::write() const
    {
        const uint32_t value = static_cast<uint32_t>(std::strtoul(m_value, nullptr, kHexRadix));
        return cube::mod().write<uint32_t>(runtimeAddress(), value);
    }
}
