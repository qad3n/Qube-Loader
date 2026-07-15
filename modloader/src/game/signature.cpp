#include "game/signature.h"
#include "game/offsets.h"
#include "core/mem.h"
#include "core/log.h"
#include "util/fmt.h"

#include <cstddef>
#include <cstdint>

namespace game::signature
{
    namespace
    {
        constexpr char kCategory[] = "signature";
        constexpr std::size_t kSigLen = 16; // prologue bytes compared per detour site
        constexpr int kNoReloc = -1; // prologue holds no base-relocated absolute-address operand
        constexpr std::size_t kRelocOperandLen = sizeof(uint32_t); // width of an imm32/disp32 operand

        // Byte offset of each site's absolute-address operand within its 16-byte prologue (skipped in
        // the compare because base relocation patches it). Derived from the disassembly, see kSites.
        constexpr int kImpactRelocOffset = 9;     // push 0x006e21cc
        constexpr int kMaxHealthRelocOffset = 10; // movss xmm3, [0x00745dc0]
        constexpr int kPickupRelocOffset = 6;     // push 0x006e5928

        struct Site
        {
            uintptr_t staticAddr;
            const char* name;
            uint8_t bytes[kSigLen];
            // Start byte of a 4-byte absolute-address operand (push imm32 / [disp32]) that the PE loader
            // patches when the image is based off 0x400000; skipped in the compare, or kNoReloc if none.
            int relocOffset;
        };

        // Reference prologue bytes captured from the RE-target Cube.exe (image base 0x400000) with
        // i686-w64-mingw32-objdump. Order matches the pinned detour targets in offsets.h. A mismatch on
        // the FIXED bytes means the loaded binary is a different build, so hooking would corrupt code;
        // the absolute-address operand bytes are masked (relocOffset) so a rebased-but-correct image
        // still verifies. Operands here: push 0x6e21cc (impact), movss [0x745dc0] (maxhealth), push
        // 0x6e5928 (pickup); crit/selection use register-relative operands that never relocate.
        constexpr Site kSites[] =
        {
            {off::kImpactFn, "impact",
                {0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x6a, 0xff, 0x68, 0xcc, 0x21, 0x6e, 0x00, 0x64, 0xa1, 0x00}, kImpactRelocOffset},
            {off::kCritRollFn, "crit",
                {0x55, 0x8b, 0xec, 0x51, 0x56, 0x8b, 0xf1, 0x8b, 0x96, 0x78, 0x11, 0x00, 0x00, 0x8b, 0x02, 0x3b}, kNoReloc},
            {off::kMaxHealthFn, "maxhealth",
                {0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0xf3, 0x0f, 0x10, 0x1d, 0xc0, 0x5d, 0x74, 0x00, 0x56, 0x8b}, kMaxHealthRelocOffset},
            {off::kUpdateSelectedEntityFn, "selection",
                {0x55, 0x8b, 0xec, 0x51, 0x53, 0x8b, 0xd9, 0x56, 0x8d, 0x83, 0x70, 0x0a, 0x80, 0x00, 0x50, 0x8d}, kNoReloc},
            {off::kOnItemPickupFn, "pickup",
                {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68, 0x28, 0x59, 0x6e, 0x00, 0x64, 0xa1, 0x00, 0x00, 0x00, 0x00}, kPickupRelocOffset},
        };

        constexpr std::size_t kSiteCount = sizeof(kSites) / sizeof(kSites[0]);

        const Site* siteFor(uintptr_t staticAddr)
        {
            for (const Site& site : kSites)
            {
                if (site.staticAddr == staticAddr)
                    return &site;
            }
            return nullptr;
        }

        // Compare the live bytes at a site against its reference. Guarded read (never faults); logs the
        // first differing offset on mismatch so a wrong build is easy to spot in the log.
        bool matches(const Site& site)
        {
            const uintptr_t liveAddr = mem::rebase(site.staticAddr);
            uint8_t buf[kSigLen] = {};

            if (!mem::readBytes(liveAddr, buf, kSigLen))
            {
                LOGC(Warn, kCategory, "%s @0x%08X (static 0x%08X): prologue not readable; treating as incompatible",
                     site.name, fmt::u32(liveAddr), fmt::u32(site.staticAddr));
                return false;
            }

            for (std::size_t i = 0; i < kSigLen; ++i)
            {
                // Skip the absolute-address operand: base relocation patches these bytes when the image
                // loads off 0x400000, so comparing them would false-mismatch a correct-but-rebased build.
                if (site.relocOffset >= 0)
                {
                    const std::size_t relocStart = static_cast<std::size_t>(site.relocOffset);
                    if (i >= relocStart && i < relocStart + kRelocOperandLen)
                        continue;
                }

                if (buf[i] != site.bytes[i])
                {
                    LOGC(Warn, kCategory, "%s @0x%08X: byte +%zu is 0x%02X, expected 0x%02X (wrong Cube.exe build)",
                         site.name, fmt::u32(liveAddr), i, static_cast<unsigned>(buf[i]), static_cast<unsigned>(site.bytes[i]));
                    return false;
                }
            }
            return true;
        }
    }

    bool verifyTarget(uintptr_t staticAddr)
    {
        const Site* site = siteFor(staticAddr);
        if (!site)
            return true; // not one of our pinned sites: nothing to verify

        return matches(*site);
    }

    bool compatibleBuild()
    {
        static int cached = -1; // -1 unknown, 0 mismatch, 1 match; first call runs single-threaded at load

        if (cached >= 0)
            return cached != 0;

        std::size_t mismatches = 0;
        for (const Site& site : kSites)
        {
            if (!matches(site))
                ++mismatches;
        }

        cached = (mismatches == 0) ? 1 : 0;
        if (cached)
            LOGC(Debug, kCategory, "Cube.exe matches the RE-target build (%zu detour sites verified)", kSiteCount);
        else
            LOGC(Error, kCategory, "Cube.exe does NOT match the RE-target build: %zu/%zu detour sites differ. Game-function hooks are DISABLED to avoid corrupting a different version; overlay and guarded reads still run.",
                 mismatches, kSiteCount);

        return cached != 0;
    }
}
