#include "game/chat.h"
#include "game/gamecontroller.h"
#include "game/offsets.h"
#include "core/mem.h"

#include <cstdint>

namespace game
{
    namespace
    {
        constexpr int32_t kMaxSegmentWalk = 128; // per message segment cap (a corrupt list cannot spin)
        constexpr int32_t kListWalkGuard = 64; // cycle guard on the message list (bounded anyway by kChatMaxMessages)
        constexpr uint32_t kU16SpanMax = 256; // char16 read per appendU16String call (span buffer size)
        constexpr uint16_t kAsciiPrintableLo = 0x20; // space
        constexpr uint16_t kAsciiPrintableHi = 0x7f; // one past '~'; >= this downconverts to '?'
        constexpr size_t kRgbBytes = 3; // segment color: R, G, B
        constexpr uint32_t kFnvOffsetBasis = 2166136261u; // FNV-1a 32 bit
        constexpr uint32_t kFnvPrime = 16777619u;
        constexpr uint32_t kCountMix = 2654435761u; // Knuth multiplicative hash, mixes the message count in

        // Resolves GameController to the parked ChatWidget*, validated by its vftable at object+0 (the GC
        // widget block is garbage before a world loads, so the vftable check is what keeps a stale read
        // from following a wild pointer).
        bool resolveChatWidget(uintptr_t& widgetOut)
        {
            uintptr_t gc = 0;
            if (!resolveGameController(gc))
                return false;

            uint32_t widget = 0;
            if (!mem::read(gc + off::kChatWidgetOff, widget) || !widget)
                return false;

            const uintptr_t addr = static_cast<uintptr_t>(widget);
            if (!mem::readable(reinterpret_cast<const void*>(addr), off::kChatInputActiveOff + 1))
                return false;

            uint32_t vtable = 0;
            if (!mem::read(addr, vtable))
                return false;
            if (vtable != static_cast<uint32_t>(mem::rebase(off::kChatWidgetVtableA)) &&
                vtable != static_cast<uint32_t>(mem::rebase(off::kChatWidgetVtableB)))
                return false;

            widgetOut = addr;
            return true;
        }

        // Appends an MSVC 32 bit std::u16string (at strBase) to out, low byte downconverted to printable
        // ASCII (non printable to '?'), matching how player/item names are read. Returns the new length.
        uint32_t appendU16String(uintptr_t strBase, char* out, uint32_t outCap, uint32_t written)
        {
            if (written + 1 >= outCap)
                return written;

            uint32_t cap = 0;
            uint32_t size = 0;
            if (!mem::read(strBase + off::kStringCapOff, cap) || !mem::read(strBase + off::kStringSizeOff, size) || size == 0)
                return written;

            uintptr_t dataAddr = strBase; // inline (small string optimization)
            uint32_t maxChars = off::kStringSsoCap; // an inline string never exceeds the SSO buffer
            if (cap > off::kStringSsoCap)
            {
                uint32_t heap = 0;
                if (!mem::read(strBase, heap) || !heap)
                    return written;
                dataAddr = static_cast<uintptr_t>(heap);
                maxChars = kU16SpanMax;
            }

            uint16_t span[kU16SpanMax];
            uint32_t toRead = size < maxChars ? size : maxChars;
            if (!mem::readBytes(dataAddr, span, toRead * sizeof(uint16_t)))
                return written;

            for (uint32_t i = 0; i < toRead && written + 1 < outCap; ++i)
            {
                const uint16_t ch = span[i];
                if (ch == 0)
                    break;
                out[written++] = (ch >= kAsciiPrintableLo && ch < kAsciiPrintableHi) ? static_cast<char>(ch) : '?';
            }
            out[written] = '\0';
            return written;
        }

        // Concatenates one message node's colored segments into m.text and records the first segment color.
        void fillMessage(uint32_t msgNode, CubeChatMessage& m)
        {
            uint32_t segHead = 0;
            if (!mem::read(static_cast<uintptr_t>(msgNode) + off::kChatMsgSegListOff, segHead) || !segHead)
                return;

            uint32_t seg = 0;
            if (!mem::read(static_cast<uintptr_t>(segHead), seg)) // segHead->next = first segment
                return;

            uint32_t written = 0;
            bool gotColor = false;
            for (int32_t guard = 0; seg && seg != segHead && guard < kMaxSegmentWalk; ++guard)
            {
                written = appendU16String(static_cast<uintptr_t>(seg) + off::kChatSegTextOff, m.text, sizeof(m.text), written);

                if (!gotColor)
                {
                    uint8_t rgb[kRgbBytes] = {0, 0, 0};
                    if (mem::readBytes(static_cast<uintptr_t>(seg) + off::kChatSegColorOff, rgb, sizeof(rgb)))
                    {
                        m.colorR = rgb[0];
                        m.colorG = rgb[1];
                        m.colorB = rgb[2];
                        m.hasColor = 1;
                        gotColor = true;
                    }
                }

                uint32_t next = 0;
                if (!mem::read(static_cast<uintptr_t>(seg), next))
                    break;
                seg = next;
            }
        }
    }

    int32_t listChatMessages(CubeChatMessage* out, int32_t maxCount)
    {
        if (!out || maxCount <= 0)
            return 0;

        uintptr_t widget = 0;
        if (!resolveChatWidget(widget))
            return 0;

        uint32_t listHead = 0;
        if (!mem::read(widget + off::kChatMsgListOff, listHead) || !listHead)
            return 0;

        // Collect node addresses in list order (oldest first), capped at the game's own history size.
        uint32_t nodes[off::kChatMaxMessages];
        int32_t n = 0;
        uint32_t node = 0;
        if (!mem::read(static_cast<uintptr_t>(listHead), node)) // head->next = first (oldest)
            return 0;
        for (int32_t guard = 0; node && node != listHead && n < off::kChatMaxMessages && guard < kListWalkGuard; ++guard)
        {
            nodes[n++] = node;
            uint32_t next = 0;
            if (!mem::read(static_cast<uintptr_t>(node), next))
                break;
            node = next;
        }
        if (n == 0)
            return 0;

        // Keep the newest maxCount, emit newest last (matches the game's top to bottom draw order).
        const int32_t start = (n > maxCount) ? (n - maxCount) : 0;
        int32_t count = 0;
        for (int32_t i = start; i < n; ++i)
        {
            CubeChatMessage& m = out[count];
            m = CubeChatMessage{};
            m.structSize = sizeof(CubeChatMessage);
            fillMessage(nodes[i], m);
            ++count;
        }
        return count;
    }

    bool readChatInput(CubeChatInput& out)
    {
        out.structSize = sizeof(CubeChatInput);
        out.text[0] = '\0';
        out.active = 0;
        out.valid = 0;

        uintptr_t widget = 0;
        if (!resolveChatWidget(widget))
            return false;

        appendU16String(widget + off::kChatInputOff, out.text, sizeof(out.text), 0);

        uint8_t active = 0;
        if (mem::read(widget + off::kChatInputActiveOff, active))
            out.active = active ? 1 : 0;
        out.valid = 1;
        return true;
    }

    bool chatProbe(int32_t& countOut, uint32_t& sigOut)
    {
        countOut = 0;
        sigOut = 0;

        uintptr_t widget = 0;
        if (!resolveChatWidget(widget))
            return false;

        uint32_t count = 0;
        mem::read(widget + off::kChatMsgCountOff, count);
        if (count > static_cast<uint32_t>(off::kChatMaxMessages))
            count = 0; // garbage guard: an out of range count means the widget is not populated
        countOut = static_cast<int32_t>(count);

        if (count > 0)
        {
            CubeChatMessage newest[1] = {};
            if (listChatMessages(newest, 1) == 1)
            {
                uint32_t h = kFnvOffsetBasis; // FNV-1a over the newest line, mixed with the count
                for (const char* p = newest[0].text; *p; ++p)
                {
                    h ^= static_cast<uint8_t>(*p);
                    h *= kFnvPrime;
                }
                sigOut = h ^ (count * kCountMix);
            }
        }
        return true;
    }
}
