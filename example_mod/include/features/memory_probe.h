#pragma once
// Guarded memory probe: parses a typed hex address, rebases it, and reads/writes a u32 through the
// loader's guarded memory API. Owns the parse + read/write logic so the Mod tab's Memory view holds
// none. It only binds the text buffers and shows the results.

namespace exmod
{

    class MemoryProbe
    {
    public:
        static constexpr int kBufferSize = 16;

        char* addressBuffer() { return m_address; }
        char* valueBuffer() { return m_value; }
        int bufferSize() const { return kBufferSize; }
        bool& writeArmed() { return m_writeArmed; }

        unsigned staticAddress() const;
        unsigned runtimeAddress() const;
        bool readable() const;
        unsigned readU32() const;
        bool write() const;

    private:
        char m_address[kBufferSize] = "400000"; // image base, a safe readable demo address
        char m_value[kBufferSize] = "0";
        bool m_writeArmed = false;
    };

    MemoryProbe& memoryProbe();

}
