#pragma once
// Runtime detection of contended game-memory writes across mods. Every api setter opens a Scope
// naming the writing mod; core/mem's write observer records each write's address+owner, and a second
// mod writing the same address raises a loud (throttled) conflict warning. Last-writer-wins is the
// game's own behavior; this only makes the collision visible.
#include "cube_sdk.h"

namespace modloader::writeguard
{
    // RAII: attributes the current thread's game-memory writes to `owner` for its lifetime. Nesting
    // restores the previous writer, so a setter that internally calls another stays attributed.
    class Scope
    {
    public:
        explicit Scope(const CubeApi* owner);
        ~Scope();

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

    private:
        const CubeApi* m_previous;
    };

    // Install/remove the core/mem write observer. install() must run before any mod can write.
    void install();
    void remove();
}
