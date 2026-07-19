#pragma once
// Public contract for example_lib's inter-mod service. A consumer includes this header, resolves the
// service by kServiceName at READY, and casts the pointer to PingService. The loader never touches it.

#include "cube_sdk.h"

namespace exlib
{
    constexpr char kModId[] = "cube_mod.example_lib";
    constexpr int kPriority = 10; // dispatches before its dependents at equal ordering
    constexpr char kServiceName[] = "example_lib.ping";
    constexpr unsigned kServiceVersion = 1;
    constexpr unsigned kPingMessage = 1; // sendMessage id; the reply is payload + 1

    // The service vtable example_lib publishes; a consumer calls ping() through the resolved pointer.
    struct PingService
    {
        int (CUBE_CALL* ping)(int value);
    };
}
