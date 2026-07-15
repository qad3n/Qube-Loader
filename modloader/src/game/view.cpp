#include "game/view.h"
#include "game/gamecontroller.h"
#include "game/offsets.h"
#include "util/field.h"
#include "core/mem.h"

#include <cstdint>
#include <cstring>

namespace game
{

    bool readCamera(CubeCamera& out)
    {
        uintptr_t gc = 0;
        if (!resolveGameController(gc))
            return false;

        out.structSize = sizeof(CubeCamera);
        out.address = static_cast<uint32_t>(gc);

        field::f32(gc, off::kCamDistanceOff, out.distance);
        field::f32(gc, off::kCamPitchOff, out.pitch);
        field::f32(gc, off::kCamYawOff, out.yaw);

        out.fov = off::kCamFovRadians;
        out.valid = 1;

        const uintptr_t viewAddr = gc + off::kViewMatrixOff;
        const uintptr_t projAddr = gc + off::kProjMatrixOff;
        const size_t matrixBytes = sizeof(float) * off::kMatrixFloats;

        if (mem::readable(reinterpret_cast<const void*>(viewAddr), matrixBytes) && mem::readable(reinterpret_cast<const void*>(projAddr), matrixBytes))
        {
            std::memcpy(out.view, reinterpret_cast<const void*>(viewAddr), matrixBytes);
            std::memcpy(out.proj, reinterpret_cast<const void*>(projAddr), matrixBytes);
            out.hasMatrices = 1;
        }
        // First-person is derived: the game has no mode bool, but zoom distance sits
        // at 0 on the player's eye.
        out.firstPerson = (out.distance == 0.0f) ? 1 : 0;
        out.hasMode = 1;
        return true;
    }

    bool readDisplay(CubeDisplay& out)
    {
        uintptr_t gc = 0;
        if (!resolveGameController(gc))
            return false;

        out.structSize = sizeof(CubeDisplay);
        out.address = static_cast<uint32_t>(gc);
        field::i32(gc, off::kRenderWidthOff, out.width);
        field::i32(gc, off::kRenderHeightOff, out.height);

        int32_t fullscreen = 0;
        if (mem::read(mem::rebase(off::kFullscreenFlag), fullscreen))
            out.fullscreen = fullscreen ? 1 : 0;

        // Global settings struct (options.cfg). All int32; absent settings stay 0.
        if (mem::read(mem::rebase(off::kSettingRenderDistance), out.renderDistance) &&
            mem::read(mem::rebase(off::kSettingResX), out.resolutionX) &&
            mem::read(mem::rebase(off::kSettingResY), out.resolutionY) &&
            mem::read(mem::rebase(off::kSettingSoundVolume), out.soundVolume) &&
            mem::read(mem::rebase(off::kSettingMusicVolume), out.musicVolume) &&
            mem::read(mem::rebase(off::kSettingMinTimeStep), out.minTimeStep))
            out.hasSettings = 1;

        out.valid = 1;
        return true;
    }

    bool readAudio(CubeAudio& out)
    {
        uintptr_t gc = 0;
        if (!resolveGameController(gc))
            return false;

        out.structSize = sizeof(CubeAudio);
        // Config volumes are static globals (always present); the live streamer chain
        // is best-effort, so hasMusicState gates that part.
        mem::read(mem::rebase(off::kSettingMusicVolume), out.musicVolumeConfig);
        mem::read(mem::rebase(off::kSettingSoundVolume), out.soundVolumeConfig);

        uint32_t engine = 0;
        if (!mem::read(gc + off::kAudioEngineOff, engine) || !engine)
            return true;
        out.address = engine;

        uint32_t streamer = 0;
        if (!mem::read(engine + off::kAudioStreamerAOff, streamer) || !streamer)
            return true;
        if (!mem::readable(reinterpret_cast<const void*>(streamer), off::kMusicVolumeOff + sizeof(float)))
            return true;

        uint8_t playing = 0;
        uint8_t looping = 0;

        mem::read(streamer + off::kMusicPlayingOff, playing);
        mem::read(streamer + off::kMusicLoopingOff, looping);

        out.musicPlaying = playing ? 1 : 0;
        out.musicLooping = looping ? 1 : 0;

        field::f32(streamer, off::kMusicVolumeOff, out.musicVolumeLive);
        out.hasMusicState = 1;
        return true;
    }

    bool setCameraField(int32_t fieldId, double value)
    {
        uintptr_t gc = 0;
        if (!resolveGameController(gc))
            return false;

        const float f = static_cast<float>(value);
        switch (fieldId)
        {
            case CUBE_CAM_DISTANCE:
                return field::setF32(gc, off::kCamDistanceOff, f);
            case CUBE_CAM_PITCH:
                return field::setF32(gc, off::kCamPitchOff, f);
            case CUBE_CAM_YAW:
                return field::setF32(gc, off::kCamYawOff, f);
            default: return false;
        }
    }

    bool setDisplayField(int32_t fieldId, int32_t value)
    {
        switch (fieldId)
        {
            case CUBE_DISPLAY_FULLSCREEN:
                return mem::write<int32_t>(mem::rebase(off::kFullscreenFlag), value);
            case CUBE_DISPLAY_RESOLUTION_X:
                return mem::write<int32_t>(mem::rebase(off::kSettingResX), value);
            case CUBE_DISPLAY_RESOLUTION_Y:
                return mem::write<int32_t>(mem::rebase(off::kSettingResY), value);
            case CUBE_DISPLAY_RENDER_DISTANCE:
                return mem::write<int32_t>(mem::rebase(off::kSettingRenderDistance), value);
            case CUBE_DISPLAY_SOUND_VOLUME:
                return mem::write<int32_t>(mem::rebase(off::kSettingSoundVolume), value);
            case CUBE_DISPLAY_MUSIC_VOLUME:
                return mem::write<int32_t>(mem::rebase(off::kSettingMusicVolume), value);
            case CUBE_DISPLAY_MIN_TIMESTEP:
                return mem::write<int32_t>(mem::rebase(off::kSettingMinTimeStep), value);
            default: return false;
        }
    }
}