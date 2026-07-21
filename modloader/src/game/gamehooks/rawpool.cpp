#include "game/gamehooks/rawpool.h"
#include "game/gamehooks/gamehooks.h"
#include "hooks/detour.h"
#include "core/log.h"
#include "util/guard.h"
#include "util/inflight.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <utility>

// Generic raw-hook capture pool: precompiled detour slots per (cc, arity) so ret stack cleanup
// matches the target (no runtime codegen). __cdecl + __thiscall, int/ptr return only (float needs installRawDetour).

namespace game::gamehooks::rawpool
{
    namespace
    {
        constexpr char kCategory[] = "rawhook";

        // Per-slot in-flight counter; remove() drains it before freeing the slot's trampoline.
        std::atomic<int> g_inFlight[kSlotMax];

        struct Slot
        {
            bool inUse = false;
            uint32_t address = 0;
            CubeCallConv cc = CUBE_CC_CDECL;
            int32_t argCount = 0;
            void* trampoline = nullptr;
        };

        Slot g_slots[kSlotMax];
        std::mutex g_mutex;

        // Each arg is exposed as both int (argi) and float (argf); whichever view a handler changed
        // wins. `self` is 0 for __cdecl. Guarded (no std::exception escapes); a bad deref is still uncatchable under mingw.
        bool marshalAndDispatch(int slot, uint32_t self, const int32_t* args, CubeHookCall& call, int32_t* out)
        {
            Slot& s = g_slots[slot];
            call.structSize = sizeof(CubeHookCall);
            call.hook = CUBE_HOOK_RAW;
            call.address = s.address;
            call.self = self;
            call.argCount = s.argCount;

            for (int32_t i = 0; i < CUBE_HOOK_ARG_MAX; ++i)
            {
                call.argi[i] = args[i];
                std::memcpy(&call.argf[i], &args[i], sizeof(float)); // same 4 bytes, float view
            }

            const uint32_t address = s.address;
            bool cancel = false;
            guard::tryRunLoader("raw hook dispatch", [&]() { cancel = dispatchRaw(address, call) != 0; });

            for (int32_t i = 0; i < CUBE_HOOK_ARG_MAX; ++i)
            {
                uint32_t origBits = 0;
                uint32_t floatBits = 0;
                std::memcpy(&origBits, &args[i], sizeof(uint32_t));
                std::memcpy(&floatBits, &call.argf[i], sizeof(uint32_t));
                out[i] = (floatBits != origBits) ? static_cast<int32_t>(floatBits) : call.argi[i];
            }

            return cancel;
        }

        // __cdecl: caller-cleans, so one detour serves every arity (extra params are ignored via argCount).
        template <int SlotIndex>
        int32_t __cdecl cdeclDetour(int32_t a0, int32_t a1, int32_t a2, int32_t a3)
        {
            barrier::InFlight inflight(g_inFlight[SlotIndex]);
            const int32_t args[CUBE_HOOK_ARG_MAX] = {a0, a1, a2, a3};
            CubeHookCall call = {};
            int32_t out[CUBE_HOOK_ARG_MAX] = {};
            const bool cancel = marshalAndDispatch(SlotIndex, 0, args, call, out);
            void* trampoline = g_slots[SlotIndex].trampoline;
            if (cancel || !trampoline)
                return call.overrideReturn ? call.returnI : 0;
            typedef int32_t(__cdecl* Fn)(int32_t, int32_t, int32_t, int32_t);
            const int32_t real = reinterpret_cast<Fn>(trampoline)(out[0], out[1], out[2], out[3]);
            return call.overrideReturn ? call.returnI : real;
        }

        // __thiscall (this in ECX, callee-cleans) == __fastcall with a dummy edx on mingw; one detour per arity so ret cleanup matches.
        template <int SlotIndex>
        int32_t __fastcall thiscall0(void* ecx, void* edx)
        {
            barrier::InFlight inflight(g_inFlight[SlotIndex]);
            const int32_t args[CUBE_HOOK_ARG_MAX] = {0, 0, 0, 0};
            CubeHookCall call = {};
            int32_t out[CUBE_HOOK_ARG_MAX] = {};
            const bool cancel = marshalAndDispatch(SlotIndex, reinterpret_cast<uint32_t>(ecx), args, call, out);
            void* trampoline = g_slots[SlotIndex].trampoline;
            if (cancel || !trampoline)
                return call.overrideReturn ? call.returnI : 0;
            typedef int32_t(__fastcall* Fn)(void*, void*);
            const int32_t real = reinterpret_cast<Fn>(trampoline)(ecx, edx);
            return call.overrideReturn ? call.returnI : real;
        }

        template <int SlotIndex>
        int32_t __fastcall thiscall1(void* ecx, void* edx, int32_t a0)
        {
            barrier::InFlight inflight(g_inFlight[SlotIndex]);
            const int32_t args[CUBE_HOOK_ARG_MAX] = {a0, 0, 0, 0};
            CubeHookCall call = {};
            int32_t out[CUBE_HOOK_ARG_MAX] = {};
            const bool cancel = marshalAndDispatch(SlotIndex, reinterpret_cast<uint32_t>(ecx), args, call, out);
            void* trampoline = g_slots[SlotIndex].trampoline;
            if (cancel || !trampoline)
                return call.overrideReturn ? call.returnI : 0;
            typedef int32_t(__fastcall* Fn)(void*, void*, int32_t);
            const int32_t real = reinterpret_cast<Fn>(trampoline)(ecx, edx, out[0]);
            return call.overrideReturn ? call.returnI : real;
        }

        template <int SlotIndex>
        int32_t __fastcall thiscall2(void* ecx, void* edx, int32_t a0, int32_t a1)
        {
            barrier::InFlight inflight(g_inFlight[SlotIndex]);
            const int32_t args[CUBE_HOOK_ARG_MAX] = {a0, a1, 0, 0};
            CubeHookCall call = {};
            int32_t out[CUBE_HOOK_ARG_MAX] = {};
            const bool cancel = marshalAndDispatch(SlotIndex, reinterpret_cast<uint32_t>(ecx), args, call, out);
            void* trampoline = g_slots[SlotIndex].trampoline;
            if (cancel || !trampoline)
                return call.overrideReturn ? call.returnI : 0;
            typedef int32_t(__fastcall* Fn)(void*, void*, int32_t, int32_t);
            const int32_t real = reinterpret_cast<Fn>(trampoline)(ecx, edx, out[0], out[1]);
            return call.overrideReturn ? call.returnI : real;
        }

        template <int SlotIndex>
        int32_t __fastcall thiscall3(void* ecx, void* edx, int32_t a0, int32_t a1, int32_t a2)
        {
            barrier::InFlight inflight(g_inFlight[SlotIndex]);
            const int32_t args[CUBE_HOOK_ARG_MAX] = {a0, a1, a2, 0};
            CubeHookCall call = {};
            int32_t out[CUBE_HOOK_ARG_MAX] = {};
            const bool cancel = marshalAndDispatch(SlotIndex, reinterpret_cast<uint32_t>(ecx), args, call, out);
            void* trampoline = g_slots[SlotIndex].trampoline;
            if (cancel || !trampoline)
                return call.overrideReturn ? call.returnI : 0;
            typedef int32_t(__fastcall* Fn)(void*, void*, int32_t, int32_t, int32_t);
            const int32_t real = reinterpret_cast<Fn>(trampoline)(ecx, edx, out[0], out[1], out[2]);
            return call.overrideReturn ? call.returnI : real;
        }

        template <int SlotIndex>
        int32_t __fastcall thiscall4(void* ecx, void* edx, int32_t a0, int32_t a1, int32_t a2, int32_t a3)
        {
            barrier::InFlight inflight(g_inFlight[SlotIndex]);
            const int32_t args[CUBE_HOOK_ARG_MAX] = {a0, a1, a2, a3};
            CubeHookCall call = {};
            int32_t out[CUBE_HOOK_ARG_MAX] = {};
            const bool cancel = marshalAndDispatch(SlotIndex, reinterpret_cast<uint32_t>(ecx), args, call, out);
            void* trampoline = g_slots[SlotIndex].trampoline;
            if (cancel || !trampoline)
                return call.overrideReturn ? call.returnI : 0;
            typedef int32_t(__fastcall* Fn)(void*, void*, int32_t, int32_t, int32_t, int32_t);
            const int32_t real = reinterpret_cast<Fn>(trampoline)(ecx, edx, out[0], out[1], out[2], out[3]);
            return call.overrideReturn ? call.returnI : real;
        }

        // Precompiled detour tables, filled once from the templates via an index sequence (no codegen).
        constexpr int kMaxArity = CUBE_HOOK_ARG_MAX; // 0..kMaxArity inclusive
        static_assert(CUBE_HOOK_ARG_MAX == 4, "rawpool thiscall templates cover arity 0..4 only; add thiscallN if this changes");
        void* g_cdecl[kSlotMax] = {};
        void* g_thiscall[kMaxArity + 1][kSlotMax] = {};
        bool g_tablesReady = false;

        template <int SlotIndex>
        void fillSlotColumn()
        {
            g_cdecl[SlotIndex] = reinterpret_cast<void*>(&cdeclDetour<SlotIndex>);
            g_thiscall[0][SlotIndex] = reinterpret_cast<void*>(&thiscall0<SlotIndex>);
            g_thiscall[1][SlotIndex] = reinterpret_cast<void*>(&thiscall1<SlotIndex>);
            g_thiscall[2][SlotIndex] = reinterpret_cast<void*>(&thiscall2<SlotIndex>);
            g_thiscall[3][SlotIndex] = reinterpret_cast<void*>(&thiscall3<SlotIndex>);
            g_thiscall[4][SlotIndex] = reinterpret_cast<void*>(&thiscall4<SlotIndex>);
        }

        template <std::size_t... S>
        void fillTables(std::index_sequence<S...>)
        {
            (fillSlotColumn<static_cast<int>(S)>(), ...);
        }

        void ensureTables()
        {
            if (g_tablesReady)
                return;
            fillTables(std::make_index_sequence<kSlotMax>{});
            g_tablesReady = true;
        }

        void* detourFor(CubeCallConv cc, int32_t argCount, int slot)
        {
            switch (cc)
            {
                case CUBE_CC_CDECL:
                    return g_cdecl[slot];
                case CUBE_CC_THISCALL:
                    return g_thiscall[argCount][slot];
                default:
                    return nullptr; // stdcall/fastcall not supported by the generic pool
            }
        }
    }

    bool install(uint32_t address, CubeCallConv cc, int32_t argCount)
    {
        if (!address || argCount < 0 || argCount > kMaxArity)
            return false;
        if (cc != CUBE_CC_CDECL && cc != CUBE_CC_THISCALL)
        {
            LOGC(Warn, kCategory, "0x%08X: convention %d not supported by the generic pool (use installRawDetour)", address, static_cast<int>(cc));
            return false;
        }

        int slot = -1;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            ensureTables();
            for (int i = 0; i < kSlotMax; ++i)
            {
                if (!g_slots[i].inUse)
                {
                    slot = i;
                    break;
                }
            }

            if (slot < 0)
            {
                LOGC(Warn, kCategory, "0x%08X: no free generic slot (%d in use)", address, kSlotMax);
                return false;
            }

            g_slots[slot].inUse = true;
            g_slots[slot].address = address;
            g_slots[slot].cc = cc;
            g_slots[slot].argCount = argCount;
            g_slots[slot].trampoline = nullptr;
        }

        void* detour = detourFor(cc, argCount, slot);
        if (!hooks::detour::create(reinterpret_cast<void*>(static_cast<uintptr_t>(address)), detour, &g_slots[slot].trampoline))
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_slots[slot] = Slot{};
            return false;
        }

        LOGC(Debug, kCategory, "generic slot %d armed at 0x%08X (cc %d, %d args, trampoline 0x%08X)", slot, address, static_cast<int>(cc), argCount, static_cast<uint32_t>(reinterpret_cast<uintptr_t>(g_slots[slot].trampoline)));
        return true;
    }

    bool remove(uint32_t address)
    {
        int slot = -1;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            for (int i = 0; i < kSlotMax; ++i)
            {
                if (g_slots[i].inUse && g_slots[i].address == address)
                {
                    slot = i;
                    break;
                }
            }
        }

        if (slot < 0)
            return false;

        // Wait out any in-flight call through this slot before we free its trampoline, then remove.
        barrier::drain(g_inFlight[slot], "raw hook");
        hooks::detour::remove(reinterpret_cast<void*>(static_cast<uintptr_t>(address)));
        std::lock_guard<std::mutex> lock(g_mutex);
        g_slots[slot] = Slot{};
        return true;
    }

    bool config(uint32_t address, CubeCallConv& ccOut, int32_t& argCountOut)
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (int i = 0; i < kSlotMax; ++i)
        {
            if (g_slots[i].inUse && g_slots[i].address == address)
            {
                ccOut = g_slots[i].cc;
                argCountOut = g_slots[i].argCount;
                return true;
            }
        }
        return false;
    }

    void shutdown()
    {
        for (int i = 0; i < kSlotMax; ++i)
        {
            uint32_t address = 0;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                if (!g_slots[i].inUse)
                    continue;
                address = g_slots[i].address;
            }
            remove(address);
        }
    }
}