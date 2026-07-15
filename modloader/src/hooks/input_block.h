#pragma once
// Freezes game input while an overlay is open (client only) by IAT-hooking GetFocus to report "not
// focused", so the game's `if (GetFocus() == gameWindow)` input+recenter block is skipped. On the
// block edge it also suspends the game's DirectInput acquisition (see hooks::dinput) so window
// messages reach the overlay, and forces the OS cursor visible. No cursor clip/warp (it only fought
// the Wayland compositor and desynced the pointer).

namespace hooks::input_block
{
    bool install(); // IAT-hook GetFocus (+ SetCursorPos) so the game reads no input while blocked
    void remove(); // restore the IAT slots (call before detour::shutdown)
    void setBlocked(bool blocked); // true: freeze game input + free visible cursor; false: restore
    bool blocked();
}