#include "game/game.h"
#include "game/offsets.h"
#include "core/mem.h"
#include "core/log.h"
#include "util/fmt.h"

#include <cstdint>
#include <cstring>

namespace game
{
    namespace
    {
        constexpr char kCategory[] = "game";
        constexpr int32_t kPreviewDwords = 8;
        constexpr int32_t kBytesPerDword = 4;

        void hexDump(const char* label, uintptr_t liveAddr, int32_t dwords)
        {
            LOGC(Debug, kCategory, "%s @ 0x%08X:", label, fmt::u32(liveAddr));
            for (int32_t i = 0; i < dwords; ++i)
            {
                const uintptr_t addr = liveAddr + static_cast<uintptr_t>(i) * kBytesPerDword;
                uint32_t value = 0;
                if (!mem::read(addr, value))
                {
                    LOGC(Warn, kCategory, "  +0x%02X  <unreadable>", i * kBytesPerDword);
                    continue;
                }
                float asFloat = 0.0f;
                std::memcpy(&asFloat, &value, sizeof(asFloat));
                LOGC(Debug, kCategory, "  +0x%02X  0x%08X  int=%d  float=%.3f",
                     i * kBytesPerDword, value, static_cast<int>(value), asFloat);
            }
        }
    }

    void logModuleInfo()
    {
        const uintptr_t liveBase = mem::base();
        LOGC(Debug, kCategory, "module base = 0x%08X", fmt::u32(liveBase));
        LOGC(Debug, kCategory, "static image base = 0x%08X", fmt::u32(off::kImageBase));
        LOGC(Debug, kCategory, "ASLR slide = 0x%08X", fmt::u32(liveBase - off::kImageBase));
        LOGC(Debug, kCategory, "GameController::vfunc0 -> live 0x%08X", fmt::u32(mem::rebase(off::kGameControllerVfunc0)));
    }

    void logCandidateGlobals()
    {
        LOGC(Debug, kCategory, "--- candidate globals (identify game singletons) ---");
        for (uintptr_t staticAddr : off::kCandidateGlobals)
        {
            const uintptr_t live = mem::rebase(staticAddr);
            uint32_t value = 0;
            if (!mem::read(live, value))
            {
                LOGC(Warn, kCategory, "global 0x%08X (live 0x%08X): <unreadable>",
                     fmt::u32(staticAddr), fmt::u32(live));
                continue;
            }
            const bool looksPtr = mem::readable(reinterpret_cast<void*>(value), sizeof(uint32_t));
            LOGC(Debug, kCategory, "global 0x%08X = 0x%08X  %s",
                 fmt::u32(staticAddr), value, looksPtr ? "(valid pointer)" : "(scalar / null)");
            if (looksPtr)
                hexDump("  -> points at", value, kPreviewDwords);
        }
    }
}