#pragma once
// Every address in offsets.h comes from one specific Cube.exe (2013 alpha, 32-bit). On any other
// binary a read is garbage and a write lands in whatever occupies that offset, which is what corrupts
// a save, so the loader verifies once at boot and refuses to load on a mismatch (dllmain runMod).
//
// Identity comes from the PE header, not from the prologues of the functions the loader hooks: a
// prologue check cannot tell a wrong build apart from a correct one another tool already hooked.
#include <cstdint>

namespace game::signature
{
    struct Result
    {
        bool ok = false;
        bool headerReadable = false;
        uint32_t timeDateStamp = 0;
        uint32_t sizeOfImage = 0;
        uint32_t entryPoint = 0;
        int32_t targetsHooked = 0; // diagnostic only, never gates
        int32_t targetCount = 0;
    };

    // Called once at boot, before anything can patch the image. Logs a full report either way.
    Result verifyBuild();
}
