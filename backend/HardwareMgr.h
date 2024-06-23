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

namespace Backend {
	enum HW_ERROR {
		NONE =					0x0000,
		ALREADY_RUNNING =		0x0001,
		GENERAL_ERROR =			0x0002,
		SDL_WINDOW_INIT =		0x0003,
		GRAPHICS_INIT =			0x0004,
		GRAPHICS_START =		0x0005,
		IMGUI_INIT =			0x0006,
		GRAPHICS_INSTANCE =		0x0007,
		AUDIO_INSTANCE =		0x0008,
		CONTROL_INSTANCE =		0x0009,
		NETWORK_INSTANCE =		0x000A,
	};

	class HardwareMgr {
	public:
		// Hardware manager general member methods
		static void InitHardware(graphics_settings& _graphics_settings, audio_settings& _audio_settings, control_settings& _control_settings);
		static void ShutdownHardware();
		static HW_ERROR GetError();

		static void ProcessEvents(bool& _running);
		static void ProcessTimedEvents();
		static Sint32 GetScroll();
		static void SetMouseAlwaysVisible(const bool& _visible);

		// Graphics backend
		static void InitGraphicsBackend(virtual_graphics_information& _virt_graphics_info);
		static void DestroyGraphicsBackend();

		static graphics_settings GetGraphicsSettings();

		static bool CheckFrame();
		static void NextFrame();
		static void RenderFrame();

		static void UpdateTexture2d();
		static void SetFramerateTarget(const int& _target, const bool& _unlimited);
		static void SetSwapchainSettings(bool& _present_mode_fifo, bool& _triple_buffering);
		static void ToggleFullscreen();

		static ImFont* GetFont(const int& _index);

		// Audio backend
		static void InitAudioBackend(virtual_audio_information& _virt_audio_info);
		static void DestroyAudioBackend();

		static audio_settings GetAudioSettings();
		
		static void SetSamplingRate(int& _sampling_rate);
		static void SetVolume(const float& _volume, const float& _lfe);
		static void SetReverb(const float& _delay, const float& _decay);
		static void SetAudioChannels(const bool& _high, const bool& _low);

		// Network backend
		static void OpenNetwork(network_settings& _network_settings);
		static bool CheckNetwork();
		static void CloseNetwork();
		
		// Control backend / key presses
		static std::queue<std::pair<SDL_Keycode, bool>>& GetKeyQueue();
		static std::queue<std::tuple<int, SDL_GameControllerButton, bool>>& GetButtonQueue();

	private:
		HardwareMgr() = default;
		~HardwareMgr() {
			ShutdownHardware();

			graphicsMgr->resetInstance();
			audioMgr->resetInstance();
			controlMgr->resetInstance();
			networkMgr->resetInstance();
		}

		// instances for backend components
		static Backend::Graphics::GraphicsMgr* graphicsMgr;
		static Backend::Audio::AudioMgr* audioMgr;
		static Backend::Control::ControlMgr* controlMgr;
		static Backend::Network::NetworkMgr* networkMgr;

		static SDL_Window* window;

		// different settings structs passed on init
		static graphics_settings graphicsSettings;
		static audio_settings audioSettings;
		static control_settings controlSettings;
		static network_settings networkSettings;

		// HardwareMgr itself
		static HardwareMgr* instance;
		static HW_ERROR error;

		// framerate and timed events
		static std::mutex mutTimeDelta;
		static std::condition_variable  notifyTimeDelta;
		static std::chrono::milliseconds timePerFrame;
		static std::chrono::steady_clock::time_point timePointCur;
		static std::chrono::steady_clock::time_point timePointPrev;

		// control
		static u32 currentMouseMove;
	};
}