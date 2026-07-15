#include "api/bridge.h"
#include "core/log.h"
#include "game/view.h"
#include "game/audio.h"
#include "cube_sdk.h"

namespace modloader::api
{
    namespace
    {
        int32_t CUBE_CALL apiCameraGet(const CubeApi* api, CubeCamera* out)
        {
            return bridgeGet<CubeCamera>(api, out, &game::readCamera, "camera.get", "unavailable", &CubeCamera::address);
        }

        int32_t CUBE_CALL apiCameraSet(const CubeApi* api, int32_t field, double value)
        {
            return bridgeSetIndexed(api, "camera.set", &game::setCameraField, field, value);
        }

        int32_t CUBE_CALL apiDisplayGet(const CubeApi* api, CubeDisplay* out)
        {
            return bridgeGet<CubeDisplay>(api, out, &game::readDisplay, "display.get", "unavailable", &CubeDisplay::address);
        }

        int32_t CUBE_CALL apiDisplaySet(const CubeApi* api, int32_t field, int32_t value)
        {
            return bridgeSetPair(api, "display.set", &game::setDisplayField, field, value);
        }

        int32_t CUBE_CALL apiAudioGet(const CubeApi* api, CubeAudio* out)
        {
            return bridgeGet<CubeAudio>(api, out, &game::readAudio, "audio.get", "unavailable", &CubeAudio::address);
        }

        int32_t CUBE_CALL apiAudioPlaySound(const CubeApi* api, int32_t soundId)
        {
            const bool ok = game::playSound(soundId);
            LOGC(Debug, kApiCategory, "'%s' audio.playSound(%d) -> %s", modName(api), soundId, ok ? "ok" : "fail");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiAudioPlaySoundAt(const CubeApi* api, int32_t soundId, float x, float y, float z, float volume, float pitch)
        {
            const bool ok = game::playSoundAt(soundId, x, y, z, volume, pitch);
            LOGC(Debug, kApiCategory, "'%s' audio.playSoundAt(%d) -> %s", modName(api), soundId, ok ? "ok" : "fail");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiAudioStopMusic(const CubeApi* api)
        {
            const bool ok = game::stopMusic();
            LOGC(Debug, kApiCategory, "'%s' audio.stopMusic -> %s", modName(api), ok ? "ok" : "fail");
            return okInt(ok);
        }

        int32_t CUBE_CALL apiAudioSetMusicVolume(const CubeApi* api, float volume)
        {
            const bool ok = game::setMusicVolume(volume);
            LOGC(Debug, kApiCategory, "'%s' audio.setMusicVolume(%.2f) -> %s", modName(api), volume, ok ? "ok" : "fail");
            return okInt(ok);
        }

    }

    void fillView(CubeApi& api)
    {
        api.camera.get = &apiCameraGet;
        api.camera.set = &apiCameraSet;
        api.display.get = &apiDisplayGet;
        api.display.set = &apiDisplaySet;
        api.audio.get = &apiAudioGet;
        api.audio.playSound = &apiAudioPlaySound;
        api.audio.playSoundAt = &apiAudioPlaySoundAt;
        api.audio.stopMusic = &apiAudioStopMusic;
        api.audio.setMusicVolume = &apiAudioSetMusicVolume;
    }

}
