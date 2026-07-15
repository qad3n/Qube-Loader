#include "game/audio.h"
#include "game/gamecontroller.h"
#include "game/offsets.h"
#include "core/mem.h"
#include "util/guard.h"

#include <cstdint>

namespace game
{
    namespace
    {
        // GameController sfx methods are __thiscall; on mingw that is __fastcall with an unused EDX.
        typedef void(__fastcall* PlaySound2DFn)(void* gc, void* edx, int32_t soundId);
        typedef void(__fastcall* PlaySoundPosFn)(void* gc, void* edx, int32_t soundId, int64_t* pos, float volume, float pitch);
        // XAudio2Engine vtable methods, __thiscall this=engine.
        typedef void(__fastcall* AudioVoidFn)(void* engine, void* edx);
        typedef void(__fastcall* AudioVolumeFn)(void* engine, void* edx, float a, float b);

        bool soundIdValid(int32_t soundId)
        {
            return soundId >= off::kSoundIdMin && soundId <= off::kSoundIdMax;
        }

        // Resolves the live XAudio2Engine pointer and its vtable method at slotOffset. False if any
        // read fails (title screen, unmapped engine).
        bool resolveEngineMethod(uintptr_t slotOffset, void*& engineOut, void*& fnOut)
        {
            uintptr_t gc = 0;
            if (!resolveGameController(gc))
                return false;

            uint32_t engine = 0;
            if (!mem::read(gc + off::kAudioEngineOff, engine) || !engine)
                return false;

            uint32_t vtable = 0;
            if (!mem::read(engine, vtable) || !vtable)
                return false;

            uint32_t fn = 0;
            if (!mem::read(vtable + slotOffset, fn) || !fn)
                return false;

            engineOut = reinterpret_cast<void*>(engine);
            fnOut = reinterpret_cast<void*>(fn);
            return true;
        }

    }

    bool playSound(int32_t soundId)
    {
        if (!soundIdValid(soundId))
            return false;

        uintptr_t gc = 0;
        if (!resolveGameController(gc))
            return false;

        const PlaySound2DFn fn = reinterpret_cast<PlaySound2DFn>(mem::rebase(off::kPlaySound2DFn));
        guard::tryRun("playSound", [&]() { fn(reinterpret_cast<void*>(gc), nullptr, soundId); });
        return true;
    }

    bool playSoundAt(int32_t soundId, float x, float y, float z, float volume, float pitch)
    {
        if (!soundIdValid(soundId))
            return false;

        uintptr_t gc = 0;
        if (!resolveGameController(gc))
            return false;

        int64_t pos[3];
        pos[0] = static_cast<int64_t>(static_cast<double>(x) * off::kPlayerPosScale);
        pos[1] = static_cast<int64_t>(static_cast<double>(y) * off::kPlayerPosScale);
        pos[2] = static_cast<int64_t>(static_cast<double>(z) * off::kPlayerPosScale);

        const PlaySoundPosFn fn = reinterpret_cast<PlaySoundPosFn>(mem::rebase(off::kPlaySoundPosFn));
        guard::tryRun("playSoundAt", [&]() { fn(reinterpret_cast<void*>(gc), nullptr, soundId, pos, volume, pitch); });
        return true;
    }

    bool stopMusic()
    {
        void* engine = nullptr;
        void* fn = nullptr;
        if (!resolveEngineMethod(off::kAudioVtblStopMusic, engine, fn))
            return false;

        const AudioVoidFn call = reinterpret_cast<AudioVoidFn>(fn);
        guard::tryRun("stopMusic", [&]() { call(engine, nullptr); });
        return true;
    }

    bool setMusicVolume(float volume)
    {
        void* engine = nullptr;
        void* fn = nullptr;
        if (!resolveEngineMethod(off::kAudioVtblSetMusicVolume, engine, fn))
            return false;

        const AudioVolumeFn call = reinterpret_cast<AudioVolumeFn>(fn);
        guard::tryRun("setMusicVolume", [&]() { call(engine, nullptr, volume, volume); });
        return true;
    }

}
