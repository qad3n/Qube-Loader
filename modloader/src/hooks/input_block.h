#pragma once
// Coordinates the overlay's input freeze (client only). The camera is NOT driven by the cursor: the
// game reads its mouse device's relative axes through DirectInput and feeds them straight to the
// camera, so movement, actions and look are all frozen at the source by hooks::dinput. What the cursor
// APIs do is position the pointer, and this module owns them for the duration of the freeze:
//   SetCursorPos - the game warps the pointer to the client center every look mode frame. Swallowed
//                  while a menu is open, so the pointer is free to reach the widgets.
//   GetCursorPos - the game reads the pointer as an absolute position in its own cursor driven screens
//                  (inventory and friends) and can rotate the camera from its frame to frame delta.
//                  Reported frozen at the game's own anchor while a menu is open, so that path is still.
// Both are patched in the game's import table only, so the loader's own calls (ImGui's mouse position
// among them) still see the real pointer.

namespace hooks::input_block
{
    bool install(); // IAT hook Get/SetCursorPos so the game's pointer handling freezes while blocked
    void remove(); // restore the IAT slots (call before detour::shutdown)
    void setBlocked(bool blocked); // true: freeze game input + free visible cursor; false: restore
}
