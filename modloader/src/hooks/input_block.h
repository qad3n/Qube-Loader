#pragma once
// Coordinates the overlay's input freeze (client only). Movement and camera are blocked at the source
// by zeroing the game's DirectInput reads (see hooks::dinput); this module frees the OS cursor for the
// menu by IAT hooking the game's mouse look recenter (user32 SetCursorPos) and swallowing it while a
// menu is open, and drives the DirectInput block + cursor visibility on the block edge.

namespace hooks::input_block
{
    bool install(); // IAT hook SetCursorPos so the game's camera recenter is suppressed while blocked
    void remove(); // restore the IAT slot (call before detour::shutdown)
    void setBlocked(bool blocked); // true: freeze game input + free visible cursor; false: restore
    bool blocked();
}
