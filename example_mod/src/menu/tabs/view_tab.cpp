#include "menu/tabs/view_tab.h"
#include "mod_context.h"

#include "cube_mod.hpp"
#include "imgui.h"

namespace exmod::menu
{
    namespace
    {

        constexpr int kMatrixSide = 4;
        constexpr int kMaxVolume = 100;
        constexpr int kMaxSoundId = 100; // 101 built in sfx ids (CUBE_CATALOG_SOUND)
        constexpr int kResolutionMin = 1;
        constexpr int kResolutionMax = 16384;
        constexpr int kRenderDistMin = 1;
        constexpr int kRenderDistMax = 100000;
        constexpr int kTimeStepMin = 1;
        constexpr int kTimeStepMax = 1000;

    }

    void ViewTab::drawMatrix(const char* label, const float* matrix)
    {
        if (!ImGui::TreeNode(label))
            return;
        for (int r = 0; r < kMatrixSide; ++r)
        {
            const float* rowPtr = matrix + r * kMatrixSide;
            ImGui::Text("%8.2f %8.2f %8.2f %8.2f", rowPtr[0], rowPtr[1], rowPtr[2], rowPtr[3]);
        }
        ImGui::TreePop();
    }

    void ViewTab::drawCamera()
    {
        cube::Camera cam(g_api);
        if (!cam.valid())
        {
            ImGui::TextDisabled("(unavailable)");
            return;
        }
        addressHeader("GameController", cam.getAddress());
        if (beginTable("vw_cam"))
        {
            row("FOV", "%.3f rad", cam.getFov());
            row("Matrices", "%s", cam.hasMatrices() ? "live" : "n/a");
            if (cam.hasMode())
                row("Mode", "%s", cam.isFirstPerson() ? "first person" : "third person");
            ImGui::EndTable();
        }
        float zoom = cam.getDistance();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::DragFloat("zoom", &zoom, kFineDragSpeed))
            cam.setDistance(zoom);
        float pitch = cam.getPitch();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::DragFloat("pitch", &pitch, kStatDragSpeed))
            cam.setPitch(pitch);
        float yaw = cam.getYaw();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::DragFloat("yaw", &yaw, kStatDragSpeed))
            cam.setYaw(yaw);
        if (cam.hasMatrices())
        {
            drawMatrix("view matrix", cam.getViewMatrix());
            drawMatrix("proj matrix", cam.getProjMatrix());
        }
    }

    void ViewTab::drawDisplay()
    {
        cube::Display disp(g_api);
        if (!disp.valid())
        {
            ImGui::TextDisabled("(unavailable)");
            return;
        }
        addressHeader("GameController", disp.getAddress());
        if (beginTable("vw_disp"))
        {
            row("Backbuffer", "%d x %d (live render target)", disp.getWidth(), disp.getHeight());
            ImGui::EndTable();
        }
        if (!disp.hasSettings())
            return;
        ImGui::SeparatorText("edit settings");
        bool fullscreen = disp.isFullscreen();
        if (ImGui::Checkbox("fullscreen", &fullscreen))
            disp.setFullscreen(fullscreen);
        int resolutionX = disp.getResolutionX();
        int resolutionY = disp.getResolutionY();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        const bool resXChanged = ImGui::DragInt("resolution x", &resolutionX, kIntDragSpeed, kResolutionMin, kResolutionMax, "%d", kClampFlags);
        ImGui::SetNextItemWidth(sc(kInputWidth));
        const bool resYChanged = ImGui::DragInt("resolution y", &resolutionY, kIntDragSpeed, kResolutionMin, kResolutionMax, "%d", kClampFlags);
        if (resXChanged || resYChanged)
            disp.setResolution(resolutionX, resolutionY);
        int renderDistance = disp.getRenderDistance();
        if (dragInt("render distance", renderDistance, kIntDragSpeed, kRenderDistMin, kRenderDistMax))
            disp.setRenderDistance(renderDistance);
        int soundVolume = disp.getSoundVolume();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::SliderInt("sound volume", &soundVolume, 0, kMaxVolume))
            disp.setSoundVolume(soundVolume);
        int musicVolume = disp.getMusicVolume();
        ImGui::SetNextItemWidth(sc(kInputWidth));
        if (ImGui::SliderInt("music volume", &musicVolume, 0, kMaxVolume, "%d", kClampFlags))
            disp.setMusicVolume(musicVolume);
        int minTimeStep = disp.getMinTimeStep();
        if (dragInt("min timestep", minTimeStep, kIntDragSpeed, kTimeStepMin, kTimeStepMax))
            disp.setMinTimeStep(minTimeStep);
    }

    void ViewTab::drawAudio()
    {
        cube::Audio audio(g_api);
        if (!audio.valid())
        {
            ImGui::TextDisabled("(unavailable)");
            return;
        }
        addressHeader("XAudio2Engine", audio.getAddress());
        if (beginTable("vw_audio"))
        {
            row("Music volume (cfg)", "%d", audio.getMusicVolumeConfig());
            row("Sound volume (cfg)", "%d (saved only; unused live)", audio.getSoundVolumeConfig());
            if (audio.hasMusicState())
            {
                row("Music playing", "%s", yesNo(audio.isMusicPlaying()));
                row("Music looping", "%s", yesNo(audio.isMusicLooping()));
                row("Music volume (live)", "%.2f", audio.getMusicVolumeLive());
            }
            else
                row("Music streamer", "%s", "(not resolved)");
            ImGui::EndTable();
        }
        ImGui::SeparatorText("play sound (game call)");
        ImGui::SetNextItemWidth(sc(kInputWidth));
        ImGui::DragInt("sound id", &m_soundId, kIntDragSpeed, 0, kMaxSoundId, "%d", kClampFlags);
        const char* soundName = cube::catalog::name(g_api, CUBE_CATALOG_SOUND, m_soundId);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", soundName ? soundName : "?");
        if (ImGui::Button("Play sound"))
            audio.playSound(m_soundId);
        ImGui::SameLine();
        if (ImGui::Button("Stop music"))
            audio.stopMusic();
        ImGui::TextDisabled("saved volume: edit in Display; live music volume via setMusicVolumeLive.");
    }

    void ViewTab::draw(const CubeEventArgs&)
    {
        if (!ImGui::BeginTabBar("##viewtabs"))
            return;
        if (ImGui::BeginTabItem("Camera"))
        {
            drawCamera();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Display"))
        {
            drawDisplay();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Audio"))
        {
            drawAudio();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

}
