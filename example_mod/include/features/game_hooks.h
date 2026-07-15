#pragma once
// Owns the mod's INTERCEPT side: installs the built in impact/crit/max HP game function hooks and
// exposes their toggles + counters. The Hooks tab binds widgets to settings() and drives detach/
// reattach, raw and rawDetour installs through here; every interception handler (which runs on the
// game thread) lives here, not in the menu.

#include "cube_mod.hpp"

#include <atomic>
#include <cstdint>

namespace exmod
{

    constexpr float kDefaultDamageScale = 0.5f;
    constexpr float kDefaultMaxHpScale = 2.0f;
    constexpr int kRawAddressLen = 16;

    class GameHooks
    {
    public:
        // Toggles read on the game thread, written by the UI (benign races; counters are atomic).
        struct Settings
        {
            bool cancelImpact = false; // negate every incoming hit (invincible)
            bool scaleDamage = false; // multiply incoming damage by damageScale
            float damageScale = kDefaultDamageScale;
            bool forceCrit = false; // force every crit roll to succeed
            bool denyCrit = false; // force every crit roll to fail
            bool scaleMaxHp = false; // multiply computed max HP by maxHpScale
            float maxHpScale = kDefaultMaxHpScale;
        };

        // Raw hook installer inputs. The menu binds widgets to these; the parse + install logic lives
        // here (installRawFromInput), so the menu never touches an address or calling convention.
        struct RawInput
        {
            char address[kRawAddressLen] = ""; // seeded at install() from the loader's built in crit target
            int conv = 0; // index into the menu's convention dropdown (maps to cube::CallConv)
            int argCount = 0;
        };

        // Register the built in interception handlers (impact / crit / max HP) once at init.
        void install();

        Settings& settings() { return m_settings; }

        unsigned impactHits() const { return m_impactHits.load(); }
        unsigned critRolls() const { return m_critRolls.load(); }
        unsigned maxHpCalls() const { return m_maxHpCalls.load(); }
        unsigned rawCalls() const { return m_rawCalls.load(); }
        unsigned rawDetourCalls() const;

        // Last intercepted impact, captured via the HookCall inspection API (addresses + args only,
        // no game offsets, those belong to the loader). hasLastImpact() is false until one fires.
        bool hasLastImpact() const { return m_lastImpactValid.load(); }
        unsigned lastImpactSelf() const { return m_lastImpactSelf.load(); }
        unsigned lastImpactTarget() const { return m_lastImpactTarget.load(); }
        int lastImpactDamage() const { return m_lastImpactDamage.load(); }

        // Detach / reattach a built in hook (EventListener/EventHook removal showcase).
        bool builtinAttached(cube::Hook hook) const;
        void setBuiltinAttached(cube::Hook hook, bool attached);

        // Raw hook (generic pool). The menu binds to rawInput() and calls these; parsing the hex
        // address + convention happens here, not in the menu. Auto manages the built in crit hook when
        // the target is the crit roll address.
        RawInput& rawInput() { return m_rawInput; }
        bool installRawFromInput();
        void removeRawFromInput();
        bool rawInstalled() const { return m_rawInstalled; }

        // rawDetour: install a hand written detour on the crit roll function (float/odd arity escape
        // hatch). Borrows the crit slot from the built in crit hook while installed.
        bool installRawDetour();
        void removeRawDetour();
        bool rawDetourInstalled() const { return m_rawDetourInstalled; }

    private:
        enum class CritOwner { None, RawPool, RawDetour };

        void impactHandler(cube::HookCall& call);
        void critHandler(cube::HookCall& call);
        void maxHpHandler(cube::HookCall& call);
        void attachHook(cube::Hook hook); // reregister the built in handler for this hook
        bool borrowCrit(CritOwner who); // detach built in crit so a raw/detour hook can own the slot
        void returnCrit(); // reattach the built in crit hook
        bool installRaw(uint32_t address, cube::CallConv conv, int argCount);
        void removeRaw(uint32_t address);

        Settings m_settings;
        RawInput m_rawInput;
        std::atomic<unsigned> m_impactHits{0};
        std::atomic<unsigned> m_critRolls{0};
        std::atomic<unsigned> m_maxHpCalls{0};
        std::atomic<unsigned> m_rawCalls{0};
        std::atomic<bool> m_lastImpactValid{false};
        std::atomic<unsigned> m_lastImpactSelf{0};
        std::atomic<unsigned> m_lastImpactTarget{0};
        std::atomic<int> m_lastImpactDamage{0};
        bool m_impactAttached = false;
        bool m_critAttached = false;
        bool m_maxHpAttached = false;
        bool m_rawInstalled = false;
        bool m_rawDetourInstalled = false;
        CritOwner m_critOwner = CritOwner::None;
    };

    GameHooks& gameHooks();

}
