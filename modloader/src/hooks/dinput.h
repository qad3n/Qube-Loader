#pragma once
// Blocks the game's DirectInput reads while an overlay owns input. The game polls its keyboard device
// (movement + action keys) and mouse device (camera look + buttons) through IDirectInputDevice8::
// GetDeviceState every frame; we hook that one call and, while blocked, hand back an all zero buffer.
// That freezes movement and camera at the exact point the game reads them, WITHOUT unacquiring the
// device, so on Windows (where the game holds the mouse non exclusively) window messages still reach
// the overlay's normal WndProc input path.
//
// Unblocking swallows a few more MOUSE reads before letting the camera move again (see the settle
// comment in the .cpp): the game recenters the pointer every look mode frame, that recenter is
// suppressed for as long as the menu is open, and the catch up warp on the frame after it closes
// arrives as one huge relative delta that would otherwise spin the camera and pin its pitch clamp.

namespace hooks::dinput
{
    bool install(); // hook IDirectInputDevice8::GetDeviceState (shared vtable)
    void remove(); // clear the block, then remove the hook
    void setBlocked(bool blocked); // true: zero every GetDeviceState result (menu open); false: pass through
}
