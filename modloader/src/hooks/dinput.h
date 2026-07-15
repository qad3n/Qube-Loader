#pragma once
// Suspends the game's DirectInput mouse+keyboard acquisition while an overlay is open. wine: an
// acquired DI mouse swallows the WM_*BUTTON messages ImGui reads, so unacquiring the game's devices
// lets Wine deliver mouse+keyboard window messages again and the overlay's normal input path works.
// The game holds its devices acquired for the whole session and does not re-acquire on its own, so
// re-acquiring on menu close hands input back cleanly.

namespace hooks::dinput
{
    bool install(); // hook IDirectInputDevice8::GetDeviceState (shared vtable) to capture the game's devices
    void remove(); // re-acquire if suspended, then remove the hook
    void setAcquired(bool acquired); // false: unacquire captured devices (menu open); true: re-acquire (menu close)
}
