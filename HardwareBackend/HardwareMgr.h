#pragma once

#include "pch.h"
#include "framework.h"

#include <chrono>
#include <mutex>

#include "defs.h"
#include "logger.h"

#ifndef HWMGR_INCLUDE
#define HWMGR_INCLUDE
#define SDL_MAIN_HANDLED
#include "../submodules/SDL/include/SDL.h"
#include "../submodules/imgui/imgui.h"
#include "../submodules/imgui/backends/imgui_impl_sdl2.h"
#include "GraphicsMgr.h"
#include "AudioMgr.h"
#include "HardwareTypes.h"
#include "ControlMgr.h"
#include "NetworkMgr.h"
#include "data_io.h"
#include "FileMapper.h"
#endif

#define HWMGR_ERR_ALREADY_RUNNING		0x00000001

namespace Backend {
	class HardwareMgr {
	public:
		static u8 InitHardware(graphics_settings& _graphics_settings, audio_settings& _audio_settings, control_settings& _control_settings);
		static void ShutdownHardware();
		static void NextFrame();
		static void RenderFrame();
		static void ProcessEvents(bool& _running);
		static void ToggleFullscreen();

		static void InitGraphicsBackend(virtual_graphics_information& _virt_graphics_info);
		static void InitAudioBackend(virtual_audio_information& _virt_audio_info);
		static void DestroyGraphicsBackend();
		static void DestroyAudioBackend();

		static void UpdateGpuData();

		static Sint32 GetScroll();

		static void GetGraphicsSettings(graphics_settings& _graphics_settings);

		static void SetFramerateTarget(const int& _target, const bool& _unlimited);

		static void SetSwapchainSettings(bool& _present_mode_fifo, bool& _triple_buffering);

		static void SetSamplingRate(int& _sampling_rate);

		static void SetVolume(const float& _volume, const float& _lfe);
		static void SetReverb(const float& _delay, const float& _decay);

		static void GetAudioSettings(audio_settings& _audio_settings);

		static void ProcessTimedEvents();

		static std::queue<std::pair<SDL_Keycode, bool>>& GetKeyQueue();
		static std::queue<std::tuple<int, SDL_GameControllerButton, bool>>& GetButtonQueue();

		static void SetMouseAlwaysVisible(const bool& _visible);

		static ImFont* GetFont(const int& _index);

		static void OpenNetwork(network_settings& _network_settings);
		static bool CheckNetwork();
		static void CloseNetwork();

		static void SetFrequencies(const bool& _high, const bool& _low);

		static bool CheckFrame();

	private:
		HardwareMgr() = default;
		~HardwareMgr() {
			ShutdownHardware();

			Backend::Graphics::GraphicsMgr::resetInstance();
			Backend::Audio::AudioMgr::resetInstance();
			Backend::Control::ControlMgr::resetInstance();
		}

		static Backend::Graphics::GraphicsMgr* graphicsMgr;
		static Backend::Audio::AudioMgr* audioMgr;
		static Backend::Control::ControlMgr* controlMgr;
		static Backend::Network::NetworkMgr* networkMgr;
		static SDL_Window* window;

		static graphics_settings graphicsSettings;
		static audio_settings audioSettings;
		static control_settings controlSettings;
		static network_settings networkSettings;

		static HardwareMgr* instance;
		static u32 errors;

		// for framerate target
		static std::mutex mutTimeDelta;
		static std::condition_variable  notifyTimeDelta;
		static std::chrono::milliseconds timePerFrame;
		static std::chrono::steady_clock::time_point timePointCur;
		static std::chrono::steady_clock::time_point timePointPrev;

		// control
		static u32 currentMouseMove;
	};
}