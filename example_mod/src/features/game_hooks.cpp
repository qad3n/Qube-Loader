#include "features/game_hooks.h"
#include "mod_context.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace exmod
{
    namespace
    {

        constexpr int kImpactDamageArg = 0; // argi[0] of the impact hook is the damage amount

        // Hand written detour for the rawDetour showcase. A no arg __thiscall is __fastcall(ecx, edx)
        // on mingw; edx is the unused second register slot. It just forwards to the trampoline and
        // counts, proving the bring your own detour path (float/odd arity escape hatch).
        typedef int(__fastcall* CritRollFn)(void* self, void* edx);
        CritRollFn g_critTrampoline = nullptr;
        std::atomic<unsigned> g_rawDetourCalls{0};

        int __fastcall hkCritRollDetour(void* self, void* edx)
        {
            ++g_rawDetourCalls;
            return g_critTrampoline ? g_critTrampoline(self, edx) : 0;
        }

        // The loader owns the crit roll function address; ask it rather than hardcoding an offset.
        uint32_t critRuntimeAddress()
        {
            return cube::mod().eventHook.builtinTarget(cube::Hook::CritRoll);
        }

    }

    GameHooks& gameHooks()
    {
        static GameHooks g_gameHooks;
        return g_gameHooks;
    }

    void GameHooks::impactHandler(cube::HookCall& call)
    {
        ++m_impactHits;
        m_lastImpactSelf.store(call.self());
        m_lastImpactTarget.store(call.target());
        m_lastImpactDamage.store(call.argInt(kImpactDamageArg));
        m_lastImpactValid.store(true);
        if (m_settings.cancelImpact)
            call.cancel();
        else if (m_settings.scaleDamage)
            call.setArgInt(kImpactDamageArg, static_cast<int>(call.argInt(kImpactDamageArg) * m_settings.damageScale));
    }

    void GameHooks::critHandler(cube::HookCall& call)
    {
        ++m_critRolls;
        if (m_settings.forceCrit)
            call.setReturnInt(1);
        else if (m_settings.denyCrit)
            call.setReturnInt(0);
    }

    void GameHooks::maxHpHandler(cube::HookCall& call)
    {
        ++m_maxHpCalls;
        if (m_settings.scaleMaxHp)
            call.setReturnFloat(call.returnFloat() * m_settings.maxHpScale);
    }

    void GameHooks::attachHook(cube::Hook hook)
    {
        cube::EventHook& eventHook = cube::mod().eventHook;
        switch (hook)
        {
            case cube::Hook::Impact:
                eventHook.onImpact([this](cube::HookCall& call) { impactHandler(call); });
                break;
            case cube::Hook::CritRoll:
                eventHook.onCritRoll([this](cube::HookCall& call) { critHandler(call); });
                break;
            case cube::Hook::MaxHealth:
                eventHook.onMaxHealth([this](cube::HookCall& call) { maxHpHandler(call); });
                break;
            default:
                break;
        }
    }

    void GameHooks::install()
    {
        attachHook(cube::Hook::Impact);
        m_impactAttached = true;
        attachHook(cube::Hook::CritRoll);
        m_critAttached = true;
        attachHook(cube::Hook::MaxHealth);
        m_maxHpAttached = true;

        // Seed the raw hook input with a real, loader provided hookable address (no hardcoded offset).
        const uint32_t critAddress = critRuntimeAddress();
        if (critAddress)
            std::snprintf(m_rawInput.address, sizeof(m_rawInput.address), "%X", critAddress);
    }

    bool GameHooks::builtinAttached(cube::Hook hook) const
    {
        switch (hook)
        {
            case cube::Hook::Impact:
                return m_impactAttached;
            case cube::Hook::CritRoll:
                return m_critAttached;
            case cube::Hook::MaxHealth:
                return m_maxHpAttached;
            default:
                return false;
        }
    }

    void GameHooks::setBuiltinAttached(cube::Hook hook, bool attached)
    {
        if (builtinAttached(hook) == attached)
            return;
        if (attached)
            attachHook(hook);
        else
            cube::mod().eventHook.remove(hook);
        switch (hook)
        {
            case cube::Hook::Impact:
                m_impactAttached = attached;
                break;
            case cube::Hook::CritRoll:
                m_critAttached = attached;
                break;
            case cube::Hook::MaxHealth:
                m_maxHpAttached = attached;
                break;
            default:
                break;
        }
    }

    bool GameHooks::borrowCrit(CritOwner who)
    {
        if (m_critOwner != CritOwner::None)
            return false; // the crit slot is already owned by a raw or rawDetour hook
        if (m_critAttached)
            setBuiltinAttached(cube::Hook::CritRoll, false);
        m_critOwner = who;
        return true;
    }

    void GameHooks::returnCrit()
    {
        m_critOwner = CritOwner::None;
        setBuiltinAttached(cube::Hook::CritRoll, true);
    }

    bool GameHooks::installRawFromInput()
    {
        const uint32_t address = static_cast<uint32_t>(std::strtoul(m_rawInput.address, nullptr, kHexRadix));
        return installRaw(address, static_cast<cube::CallConv>(m_rawInput.conv), m_rawInput.argCount);
    }

    void GameHooks::removeRawFromInput()
    {
        const uint32_t address = static_cast<uint32_t>(std::strtoul(m_rawInput.address, nullptr, kHexRadix));
        removeRaw(address);
    }

    bool GameHooks::installRaw(uint32_t address, cube::CallConv conv, int argCount)
    {
        if (address == 0)
            return false;
        const bool isCrit = address == critRuntimeAddress();
        if (isCrit && !borrowCrit(CritOwner::RawPool))
            return false;

        const bool ok = cube::mod().eventHook.raw(address, conv, argCount,
                                                  [this](cube::HookCall&) { ++m_rawCalls; });
        if (!ok && isCrit)
            returnCrit();
        m_rawInstalled = ok;
        return ok;
    }

    void GameHooks::removeRaw(uint32_t address)
    {
        cube::mod().eventHook.removeRaw(address);
        m_rawInstalled = false;
        if (m_critOwner == CritOwner::RawPool && address == critRuntimeAddress())
            returnCrit();
    }

    bool GameHooks::installRawDetour()
    {
        if (m_rawDetourInstalled)
            return true;
        if (!borrowCrit(CritOwner::RawDetour))
            return false;

        const uint32_t address = critRuntimeAddress();
        const bool ok = cube::mod().eventHook.rawDetour(address, reinterpret_cast<void*>(&hkCritRollDetour),
                                                        reinterpret_cast<void**>(&g_critTrampoline));
        if (!ok)
            returnCrit();
        m_rawDetourInstalled = ok;
        return ok;
    }

    void GameHooks::removeRawDetour()
    {
        if (!m_rawDetourInstalled)
            return;
        cube::mod().eventHook.removeRaw(critRuntimeAddress());
        g_critTrampoline = nullptr;
        m_rawDetourInstalled = false;
        if (m_critOwner == CritOwner::RawDetour)
            returnCrit();
    }

    unsigned GameHooks::rawDetourCalls() const
    {
        return g_rawDetourCalls.load();
    }
}
