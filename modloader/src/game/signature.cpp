#include "game/signature.h"
#include "game/offsets.h"
#include "core/mem.h"
#include "core/log.h"
#include "util/fmt.h"

#include <cstdint>

namespace game::signature
{
    namespace
    {
        constexpr char kCategory[] = "signature";

        constexpr uint32_t kRefTimeDateStamp = 0x51EA955E; // 2013-07-20 13:49:18 UTC
        constexpr uint32_t kRefSizeOfImage = 0x003BC000;
        constexpr uint32_t kRefEntryPoint = 0x0028E1E0;

        constexpr uintptr_t kElfanewOff = 0x3C;
        constexpr uint32_t kPeSignature = 0x00004550; // "PE\0\0"
        constexpr uintptr_t kCoffTimeDateStampOff = 8; // sig(4) + machine(2) + numberOfSections(2)
        constexpr uintptr_t kOptionalHeaderOff = 24; // sig(4) + COFF header(20)
        constexpr uintptr_t kOptEntryPointOff = 16;
        constexpr uintptr_t kOptSizeOfImageOff = 56;

        // First byte left behind by an inline hook: rel32 jmp (MinHook), short jmp, indirect jmp,
        // or push imm32 + ret.
        constexpr uint8_t kJmpRel32 = 0xE9;
        constexpr uint8_t kJmpRel8 = 0xEB;
        constexpr uint8_t kJmpIndirect = 0xFF;
        constexpr uint8_t kPushImm32 = 0x68;

        struct Target
        {
            uintptr_t staticAddr;
            const char* name;
        };

        constexpr Target kTargets[] = {
            {off::kApplyMeleeHitFn, "melee-hit"},
            {off::kCritRollFn, "crit"},
            {off::kStatCalcAttackDamageFn, "attackdamage"},
            {off::kUpdateSelectedEntityFn, "selection"},
            {off::kOnItemPickupFn, "pickup"},
            {off::kDbLoadBlobByKey, "asset-blob"},
        };

        constexpr int32_t kTargetCount = static_cast<int32_t>(sizeof(kTargets) / sizeof(kTargets[0]));

        // Each of these begins with a stack frame prologue in the stock binary, never a branch.
        bool looksHooked(const Target& target)
        {
            uint8_t first = 0;
            if (!mem::read(mem::rebase(target.staticAddr), first))
                return false;

            return first == kJmpRel32 || first == kJmpRel8 || first == kJmpIndirect || first == kPushImm32;
        }

        bool readPeIdentity(Result& out)
        {
            const uintptr_t base = mem::base();
            if (!base)
                return false;

            uint32_t elfanew = 0;
            if (!mem::read(base + kElfanewOff, elfanew) || !elfanew)
                return false;

            const uintptr_t pe = base + elfanew;
            uint32_t signature = 0;
            if (!mem::read(pe, signature) || signature != kPeSignature)
                return false;

            const uintptr_t opt = pe + kOptionalHeaderOff;

            return mem::read(pe + kCoffTimeDateStampOff, out.timeDateStamp) &&
                   mem::read(opt + kOptEntryPointOff, out.entryPoint) &&
                   mem::read(opt + kOptSizeOfImageOff, out.sizeOfImage);
        }

        void logMismatch(const Result& result)
        {
            LOGC(Error, kCategory, "this is NOT the Cube.exe the loader was built for:");
            LOGC(Error, kCategory, "  PE timestamp  0x%08X   expected 0x%08X", fmt::u32(result.timeDateStamp),
                 fmt::u32(kRefTimeDateStamp));
            LOGC(Error, kCategory, "  size of image 0x%08X   expected 0x%08X", fmt::u32(result.sizeOfImage),
                 fmt::u32(kRefSizeOfImage));
            LOGC(Error, kCategory, "  entry point   0x%08X   expected 0x%08X", fmt::u32(result.entryPoint),
                 fmt::u32(kRefEntryPoint));
            LOGC(Error, kCategory,
                 "every address the loader uses comes from that one build, so on this binary a read is "
                 "garbage and a write can corrupt your save.");
            LOGC(Error, kCategory, "required: the stock, unmodified 32-bit Cube.exe from the 2013 alpha.");
        }
    }

    Result verifyBuild()
    {
        Result result;
        result.targetCount = kTargetCount;
        result.headerReadable = readPeIdentity(result);

        if (!result.headerReadable)
        {
            LOGC(Error, kCategory,
                 "could not read the PE header of the host process; refusing to trust this binary");
            return result;
        }

        result.ok = result.timeDateStamp == kRefTimeDateStamp && result.sizeOfImage == kRefSizeOfImage &&
                    result.entryPoint == kRefEntryPoint;

        if (!result.ok)
        {
            logMismatch(result);
            return result;
        }

        for (const Target& target : kTargets)
        {
            if (looksHooked(target))
            {
                ++result.targetsHooked;
                LOGC(Warn, kCategory, "%s (static 0x%08X) is already hooked by another tool", target.name,
                     fmt::u32(target.staticAddr));
            }
        }

        LOGC(Info, kCategory, "Cube.exe verified (PE timestamp 0x%08X)", fmt::u32(result.timeDateStamp));

        if (result.targetsHooked > 0)
            LOGC(Warn, kCategory, "%d of %d hook targets are already patched; the loader will hook on top",
                 result.targetsHooked, result.targetCount);

        return result;
    }
}
