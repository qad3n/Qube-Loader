#pragma once
// HookCall: the interception context passed to EventHook handlers.

#include "cube/common.hpp"

namespace cube
{
    class HookCall
    {
    public:
        HookCall(CubeHookCall* call, const CubeApi* api) : m_call(call), m_api(api) {}

        Hook hook() const { return static_cast<Hook>(m_call->hook); }
        bool isRaw() const { return m_call->hook == CUBE_HOOK_RAW; }
        unsigned address() const { return m_call->address; } // hooked function (raw hooks); 0 built-in
        unsigned self() const { return m_call->self; } // 'this' object, 0 if not a method
        unsigned target() const { return m_call->target; } // decoded primary object arg (built-in)
        int argCount() const { return m_call->argCount; }

        int argInt(int index) const { return validArg(index) ? m_call->argi[index] : 0; }
        float argFloat(int index) const { return validArg(index) ? m_call->argf[index] : 0.0f; }
        void setArgInt(int index, int value)
        {
            if (validArg(index))
                m_call->argi[index] = value;
        }
        void setArgFloat(int index, float value)
        {
            if (validArg(index))
                m_call->argf[index] = value;
        }

        void cancel() { m_call->cancel = 1; } // skip the original function entirely
        bool cancelled() const { return m_call->cancel != 0; }

        // Named accessors for CUBE_HOOK_IMPACT (a real hit landing), so the participants read clearly.
        // attacker = the CombatBehavior 'this', victim = the struck creature, damage = the game's own
        // damage value (mutable), hitFlags = the hit flag word, hitContext = the hit-context pointer.
        unsigned attacker() const { return m_call->self; }
        unsigned victim() const { return m_call->target; }
        int damage() const { return m_call->argi[0]; }
        void setDamage(int value) { m_call->argi[0] = value; } // rescale the hit before it applies
        unsigned hitContext() const { return static_cast<unsigned>(m_call->argi[1]); }
        int hitFlags() const { return m_call->argi[2]; }

        int returnInt() const { return m_call->returnI; } // the original's return, unless overridden
        float returnFloat() const { return m_call->returnF; }
        void setReturnInt(int value) { m_call->returnI = value; m_call->overrideReturn = 1; }
        void setReturnFloat(float value) { m_call->returnF = value; m_call->overrideReturn = 1; }

        // Guarded reads: a bad/stale address returns 0 instead of crashing. self*/target* read at
        // self()/target()+offset; readInt/readFloat at any absolute address.
        int readInt(unsigned address) const
        {
            int value = 0;
            if (m_api)
                m_api->mem.read(m_api, address, &value, static_cast<unsigned>(sizeof(int)));
            return value;
        }

        float readFloat(unsigned address) const
        {
            float value = 0.0f;
            if (m_api)
                m_api->mem.read(m_api, address, &value, static_cast<unsigned>(sizeof(float)));
            return value;
        }

        int selfInt(unsigned offset) const { return m_call->self ? readInt(m_call->self + offset) : 0; }
        float selfFloat(unsigned offset) const { return m_call->self ? readFloat(m_call->self + offset) : 0.0f; }
        int targetInt(unsigned offset) const { return m_call->target ? readInt(m_call->target + offset) : 0; }
        float targetFloat(unsigned offset) const { return m_call->target ? readFloat(m_call->target + offset) : 0.0f; }

        CubeHookCall& raw() { return *m_call; } // escape hatch to the C struct

    private:
        bool validArg(int index) const { return index >= 0 && index < CUBE_HOOK_ARG_MAX; }
        CubeHookCall* m_call;
        const CubeApi* m_api;
    };

}
