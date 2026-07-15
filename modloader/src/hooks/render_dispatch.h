#pragma once
// Fans the single D3D9 hook out to multiple subscribers so no two consumers fight over EndScene.
// Lazily installs the d3d9 hook on the first subscribe, removes it on the last unsubscribe.

#include "hooks/d3d9_hook.h"

namespace hooks::render
{

    typedef int Token;
    constexpr Token kInvalidToken = -1;

    Token subscribe(const d3d9::Callbacks& callbacks);
    void unsubscribe(Token token);

}