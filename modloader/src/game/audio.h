#pragma once
// Sound / music playback via game-function calls (client only). Call from the game thread
// (an event callback / hook), like every other game-call.

#include <cstdint>

namespace game
{

    // Plays a built-in sound effect (CUBE_CATALOG_SOUND id, 0..100) 2D at the listener. False if the
    // id is out of range or the game is unavailable.
    bool playSound(int32_t soundId);
    // Plays a built-in sound effect at a world position, with volume/pitch multipliers.
    bool playSoundAt(int32_t soundId, float x, float y, float z, float volume, float pitch);
    // Stops the currently playing music (engine vtable slot). False if the engine is unavailable.
    bool stopMusic();
    // Sets the live music volume (0..1) on the audio engine. False if unavailable.
    bool setMusicVolume(float volume);

}
