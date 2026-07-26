#include "game/appearance.h"
#include "game/creature.h"
#include "game/offsets.h"
#include "util/field.h"
#include "core/mem.h"

#include <cstdint>

namespace game
{
    bool readAppearance(uint32_t address, CubeAppearance& out)
    {
        out.structSize = sizeof(CubeAppearance);
        out.address = 0;
        out.species = 0;
        out.gender = 0;
        out.style = 0;
        out.color0 = 0;
        out.color1 = 0;
        out.color2 = 0;
        out.valid = 0;

        uintptr_t creature = 0;
        if (!resolveCreatureOrLocal(address, creature))
            return false;

        // The appearance record is at the start of the ObjWithListMap; a null map means the creature is
        // not yet resident (title screen / mid load), not a hard failure to signal differently.
        uint32_t record = 0;
        if (!mem::read(creature + off::kAppearanceMapOff, record) || !record)
            return false;
        const uintptr_t rec = static_cast<uintptr_t>(record);

        field::i32(rec, off::kAppearanceSpeciesOff, out.species);
        field::i32(rec, off::kAppearanceStyleOff, out.style);

        int32_t genderByte = 0;
        field::byteI32(rec, off::kAppearanceGenderOff, genderByte);
        out.gender = genderByte & 1;

        field::byteI32(rec, off::kAppearanceColor0Off, out.color0);
        field::byteI32(rec, off::kAppearanceColor1Off, out.color1);
        field::byteI32(rec, off::kAppearanceColor2Off, out.color2);

        out.address = static_cast<uint32_t>(creature);
        out.valid = 1;
        return true;
    }
}
