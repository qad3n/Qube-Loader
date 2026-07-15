#pragma once

// The CubeApi bridge layer; fill() wires every bridge fn pointer, false if any left null.
struct CubeApi;

namespace modloader::api
{
    bool fill(CubeApi& api);
}
