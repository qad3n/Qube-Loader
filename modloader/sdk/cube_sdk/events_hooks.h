#pragma once
// Event + hook core value types (CubeEventArgs/Fn, CubeHook, CubeCallConv, CubeHookCall).

#include "cube_sdk/enums.h"

typedef struct CubeEventArgs
{
    uint32_t structSize;
    CubeEvent event;
    void* user;
    void* device; // IDirect3DDevice9* on FRAME
    void* hwnd; // game window on FRAME / WNDPROC
    uint32_t frameIndex;
    int32_t preReset;
    uint32_t msg; // WNDPROC message id
    uint32_t wParam; // WNDPROC wParam
    int32_t lParam; // WNDPROC lParam
    int32_t swallow; // WNDPROC out: set to 1 to consume the message
    uint32_t subject; // event subject: creature/item address for entity/target events (the item TYPE id for ITEM_PICKUP), else 0
    int32_t param; // primary int payload; meaning documented per-event (movement/coins/type id/slot)
    int32_t param2; // secondary int payload; documented per-event (previous value / second coord / delta)
    float amount; // float payload; documented per-event (damage / velocity / health), 0 when unused
} CubeEventArgs;

typedef void (CUBE_CALL* CubeEventFn)(CubeEventArgs* args);

// One directed inter-mod message (CubeServicesApi.sendMessage -> the target mod's onMessage handler).
// The payload is an opaque mod-defined blob; sender and receiver agree its meaning by msgId. Unlike the
// broadcast event bus this is point-to-point, addressed by the target mod's manifest id.
typedef struct CubeMessageArgs
{
    uint32_t structSize;
    const char* senderId; // manifest id of the sending mod (never NULL; "?" if unresolved)
    uint32_t msgId; // mod-defined message discriminator agreed by both sides
    void* payload; // opaque sender-owned buffer, valid only for the duration of the call (may be NULL)
    uint32_t payloadSize; // bytes at payload (0 if none)
    int32_t result; // out: a handler sets this; sendMessage returns the last handler's value
} CubeMessageArgs;

typedef void (CUBE_CALL* CubeMessageFn)(const struct CubeApi* api, CubeMessageArgs* args, void* user);

// Game-function hooks intercept a real game call (cancel / mutate args / override return via CubeHookCall).
// WARNING: the handler runs synchronously on the game thread - keep it small and do NOT touch the overlay.
typedef enum CubeHook
{
    CUBE_HOOK_IMPACT = 0, // hit/hard-fall lands: target=victim, argi[0]=damage, argi[1]=hit-context ptr, argi[2]=hit flags; cancel negates, mutate argi[0] to rescale
    CUBE_HOOK_CRIT_ROLL, // attacker rolls a crit: self=attacker; returnI 1=force, 0=deny
    CUBE_HOOK_MAX_HEALTH, // max health computed: self=creature; returnF rescales max HP (0 one-shot, large godmode)
    CUBE_HOOK_COUNT,
    CUBE_HOOK_RAW = 1000 // sentinel CubeHookCall.hook value for a raw (user-address) hook
} CubeHook;

// Calling convention of a raw hook target, so the loader picks the matching generic capture
// stub. The built-in (semantic) hooks do not need this - the loader knows their convention.
typedef enum CubeCallConv
{
    CUBE_CC_THISCALL = 0, // __thiscall: `this` in ECX, remaining args on the stack
    CUBE_CC_CDECL, // __cdecl: all args on the stack, caller cleans
    CUBE_CC_STDCALL, // __stdcall: all args on the stack, callee cleans
    CUBE_CC_FASTCALL // __fastcall: first two args in ECX/EDX, rest on the stack
} CubeCallConv;

// Mutable view of one intercepted call; which arg slots and return field are live is documented per CubeHook.
typedef struct CubeHookCall
{
    uint32_t structSize;
    int32_t hook; // CubeHook id, or CUBE_HOOK_RAW for a raw hook
    uint32_t address; // hooked function address (raw hooks); 0 for a built-in hook
    uint32_t self; // 'this'/ECX object address, 0 if the target is not a method
    uint32_t target; // decoded primary object argument, 0 if none (built-in hooks)
    int32_t argi[CUBE_HOOK_ARG_MAX]; // integer/pointer argument slots (mutable)
    float argf[CUBE_HOOK_ARG_MAX]; // float argument slots (mutable)
    int32_t argCount; // number of argument slots this call populates
    int32_t cancel; // out: set to 1 to skip the original function entirely
    int32_t overrideReturn; // out: set to 1 to return returnI/returnF instead of the original's
    int32_t returnI; // in/out: the original integer/pointer return, and the override value
    float returnF; // in/out: the original float return, and the override value
} CubeHookCall;

typedef void (CUBE_CALL* CubeHookFn)(CubeHookCall* call);

