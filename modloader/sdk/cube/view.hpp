#pragma once
// Client view accessors: Camera, Display, Audio.

#include "cube/common.hpp"

namespace cube
{
    class Camera
    {
    public:
        explicit Camera(const CubeApi* api) : m_api(api), m_valid(api && api->camera.get(api, &m_data) != 0) {}

        bool valid() const { return m_valid; }
        bool refresh() { m_valid = m_api && m_api->camera.get(m_api, &m_data) != 0; return m_valid; }
        float getDistance() const { return m_data.distance; }
        float getPitch() const { return m_data.pitch; }
        float getYaw() const { return m_data.yaw; }
        float getFov() const { return m_data.fov; }
        bool hasMatrices() const { return m_data.hasMatrices != 0; }
        const float* getViewMatrix() const { return m_data.view; }
        const float* getProjMatrix() const { return m_data.proj; }
        bool hasMode() const { return m_data.hasMode != 0; }
        bool isFirstPerson() const { return m_data.firstPerson != 0; }
        unsigned getAddress() const { return m_data.address; } // GameController base (raw)
        const CubeCamera& raw() const { return m_data; }
        // Live edits to the 3rd-person camera scalars.
        bool set(CameraField field, double value) const { return m_api && m_api->camera.set(m_api, static_cast<int32_t>(field), value) != 0; }
        bool setDistance(float distance) const { return set(CameraField::Distance, distance); }
        bool setPitch(float pitch) const { return set(CameraField::Pitch, pitch); }
        bool setYaw(float yaw) const { return set(CameraField::Yaw, yaw); }

    private:
        const CubeApi* m_api;
        CubeCamera m_data = {};
        bool m_valid;
    };

    class Display
    {
    public:
        explicit Display(const CubeApi* api) : m_api(api), m_valid(api && api->display.get(api, &m_data) != 0) {}

        bool valid() const { return m_valid; }
        bool refresh() { m_valid = m_api && m_api->display.get(m_api, &m_data) != 0; return m_valid; }
        int getWidth() const { return m_data.width; }
        int getHeight() const { return m_data.height; }
        bool isFullscreen() const { return m_data.fullscreen != 0; }
        bool hasSettings() const { return m_data.hasSettings != 0; }
        int getRenderDistance() const { return m_data.renderDistance; }
        int getResolutionX() const { return m_data.resolutionX; }
        int getResolutionY() const { return m_data.resolutionY; }
        int getSoundVolume() const { return m_data.soundVolume; }
        int getMusicVolume() const { return m_data.musicVolume; }
        int getMinTimeStep() const { return m_data.minTimeStep; }
        unsigned getAddress() const { return m_data.address; } // GameController base (raw)
        const CubeDisplay& raw() const { return m_data; }
        // Live edits to the graphics/display settings (the options.cfg globals).
        bool set(DisplayField field, int value) const { return m_api && m_api->display.set(m_api, static_cast<int32_t>(field), value) != 0; }
        bool setFullscreen(bool on) const { return set(DisplayField::Fullscreen, on ? 1 : 0); }
        bool setRenderDistance(int value) const { return set(DisplayField::RenderDistance, value); }
        bool setSoundVolume(int value) const { return set(DisplayField::SoundVolume, value); }
        bool setMusicVolume(int value) const { return set(DisplayField::MusicVolume, value); }
        bool setMinTimeStep(int value) const { return set(DisplayField::MinTimeStep, value); }
        bool setResolution(int x, int y) const { return set(DisplayField::ResolutionX, x) && set(DisplayField::ResolutionY, y); }

    private:
        const CubeApi* m_api;
        CubeDisplay m_data = {};
        bool m_valid;
    };

    // Live audio state + sound/music playback (client only). Playback methods are game-calls: call
    // them from an event callback / hook (the game thread). Edit saved volume via Display::setSoundVolume.
    class Audio
    {
    public:
        static constexpr float kFullVolume = 1.0f;
        static constexpr float kNormalPitch = 1.0f;

        explicit Audio(const CubeApi* api) : m_api(api), m_valid(api && api->audio.get(api, &m_data) != 0) {}

        bool valid() const { return m_valid; }
        bool refresh() { m_valid = m_api && m_api->audio.get(m_api, &m_data) != 0; return m_valid; }
        int getMusicVolumeConfig() const { return m_data.musicVolumeConfig; }
        int getSoundVolumeConfig() const { return m_data.soundVolumeConfig; }
        bool hasMusicState() const { return m_data.hasMusicState != 0; }
        bool isMusicPlaying() const { return m_data.musicPlaying != 0; }
        bool isMusicLooping() const { return m_data.musicLooping != 0; }
        float getMusicVolumeLive() const { return m_data.musicVolumeLive; }
        unsigned getAddress() const { return m_data.address; } // XAudio2Engine base (raw)
        const CubeAudio& raw() const { return m_data; }
        // Play any built-in sound effect (CUBE_CATALOG_SOUND id, 0..100), 2D at the listener.
        bool playSound(int soundId) const { return m_api && m_api->audio.playSound(m_api, soundId) != 0; }
        // Play a built-in sound at a world position with volume/pitch multipliers.
        bool playSoundAt(int soundId, const Vec3& at, float volume = kFullVolume, float pitch = kNormalPitch) const
        {
            return m_api && m_api->audio.playSoundAt(m_api, soundId, at.x, at.y, at.z, volume, pitch) != 0;
        }
        bool stopMusic() const { return m_api && m_api->audio.stopMusic(m_api) != 0; }
        bool setMusicVolumeLive(float volume) const { return m_api && m_api->audio.setMusicVolume(m_api, volume) != 0; }

    private:
        const CubeApi* m_api;
        CubeAudio m_data = {};
        bool m_valid;
    };
}
