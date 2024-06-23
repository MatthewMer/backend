#include "pch.h"
#include "framework.h"

#include "HardwareMgr.h"

using namespace std::chrono;

namespace Backend {
	HardwareMgr* HardwareMgr::instance = nullptr;
	HW_ERROR HardwareMgr::error = HW_ERROR::NONE;

	Graphics::GraphicsMgr* HardwareMgr::graphicsMgr = nullptr;
	Audio::AudioMgr* HardwareMgr::audioMgr = nullptr;
	Control::ControlMgr* HardwareMgr::controlMgr = nullptr;
	Network::NetworkMgr* HardwareMgr::networkMgr = nullptr;

	SDL_Window* HardwareMgr::window = nullptr;

	graphics_settings HardwareMgr::graphicsSettings = {};
	audio_settings HardwareMgr::audioSettings = {};
	control_settings HardwareMgr::controlSettings = {};
	network_settings HardwareMgr::networkSettings = {};

	std::chrono::milliseconds HardwareMgr::timePerFrame = std::chrono::milliseconds();
	std::mutex HardwareMgr::mutTimeDelta = std::mutex();
	std::condition_variable  HardwareMgr::notifyTimeDelta = std::condition_variable();
	steady_clock::time_point HardwareMgr::timePointCur = steady_clock::now();
	steady_clock::time_point HardwareMgr::timePointPrev = steady_clock::now();

	u32 HardwareMgr::currentMouseMove = 0;

	inline const u32 ONE_SECOND = 999;

	/* *************************************************************************************************
		INIT / DEINIT HARDWARE BACKEND
	************************************************************************************************* */
	void HardwareMgr::InitHardware(graphics_settings& _graphics_settings, audio_settings& _audio_settings, control_settings& _control_settings) {
		error = HW_ERROR::NONE;

		if (instance == nullptr) {
			instance = new HardwareMgr();
		} else {
			error = HW_ERROR::ALREADY_RUNNING;
			return;
		}

		audioSettings = _audio_settings;
		graphicsSettings = _graphics_settings;
		controlSettings = _control_settings;

		SetFramerateTarget(graphicsSettings.framerateTarget, graphicsSettings.fpsUnlimited);

		// sdl init
		window = nullptr;
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
			LOG_ERROR("[SDL]", SDL_GetError());
			error = HW_ERROR::SDL_WINDOW_INIT;
			return;
		} else {
			LOG_INFO("[SDL] initialized");
		}

		// graphics init
		graphicsMgr = Graphics::GraphicsMgr::getInstance(&window, graphicsSettings);
		if (graphicsMgr != nullptr) {
			const char* file_c = graphicsSettings.icon.c_str();
			SDL_Surface* icon = SDL_LoadBMP(file_c);

			SDL_SetWindowIcon(window, icon);

			if (!graphicsMgr->InitGraphics()) { 
				error = HW_ERROR::GRAPHICS_INIT;
				return;
			}
			if (!graphicsMgr->StartGraphics(graphicsSettings.presentModeFifo, graphicsSettings.tripleBuffering)) {
				error = HW_ERROR::GRAPHICS_START;
				return;
			}

			ImGui::CreateContext();
			ImGui::StyleColorsDark();
			if (!graphicsMgr->InitImgui()) { 
				error = HW_ERROR::IMGUI_INIT;
				return;
			}

			graphicsMgr->EnumerateShaders();
		} else {
			error = HW_ERROR::GRAPHICS_INSTANCE;
			return;
		}

		SDL_SetWindowMinimumSize(window, graphicsSettings.win_width_min, graphicsSettings.win_height_min);

		// audio init
		audioMgr = Audio::AudioMgr::getInstance();
		if (audioMgr != nullptr) {
			audioMgr->InitAudio(audioSettings, false);
		} else {
			error = HW_ERROR::AUDIO_INSTANCE;
			return;
		}

		// control inti
		controlMgr = Control::ControlMgr::getInstance();
		if (controlMgr != nullptr) {
			controlMgr->InitControl(controlSettings);
		} else {
			error = HW_ERROR::CONTROL_INSTANCE;
			return;
		}

		// network init
		networkMgr = Network::NetworkMgr::getInstance();
		if (networkMgr == nullptr) { 
			error = HW_ERROR::NETWORK_INSTANCE;
			return;
		}
	}

	void HardwareMgr::ShutdownHardware() {
		// graphics
		graphicsMgr->DestroyImgui();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();

		graphicsMgr->StopGraphics();
		graphicsMgr->ExitGraphics();

		SDL_DestroyWindow(window);
		SDL_Quit();
	}

	HW_ERROR HardwareMgr::GetError() {
		return error;
	}

	/* *************************************************************************************************
		HARDWARE EVENTS PROCESSING
	************************************************************************************************* */
	void HardwareMgr::ProcessTimedEvents() {
		steady_clock::time_point cur = steady_clock::now();
		u32 time_diff = (u32)duration_cast<milliseconds>(cur - timePointCur).count();

		timePointCur = cur;
		std::chrono::milliseconds c_time_diff = duration_cast<milliseconds>(timePointCur - timePointPrev);
		// framerate
		if (timePerFrame > c_time_diff) {
			// std::unique_lock with std::condition_variable().wait_for() actually useless here but for
			// whatever reason it consumes slightly less ressources than std::this_thread::sleep_for()
			std::unique_lock lock_fps(mutTimeDelta);
			notifyTimeDelta.wait_for(lock_fps, duration_cast<milliseconds>(timePerFrame - c_time_diff));
			//std::this_thread::sleep_for(duration_cast<microseconds>(timePerFrame - c_time_diff));
		}

		timePointPrev = steady_clock::now();


		// process mouse
		if (!controlSettings.mouse_always_visible) {
			static int x, y;
			bool visible = controlMgr->GetMouseVisible();

			if (controlMgr->CheckMouseMove(x, y)) {
				if (!visible) { controlMgr->SetMouseVisible(true); }
				currentMouseMove = 0;
			} else if (visible && currentMouseMove < ONE_SECOND * 2) {
				currentMouseMove += time_diff;
			} else {
				controlMgr->SetMouseVisible(false);
			}
		}
	}

	/* *************************************************************************************************
		GRAPHICS BACKEND
	************************************************************************************************* */
	void HardwareMgr::InitGraphicsBackend(virtual_graphics_information& _virt_graphics_info) {
		graphicsMgr->InitGraphicsBackend(_virt_graphics_info);
	}

	void HardwareMgr::DestroyGraphicsBackend() {
		graphicsMgr->DestroyGraphicsBackend();
	}

	void HardwareMgr::GetGraphicsSettings(graphics_settings _graphics_settings) {
		_graphics_settings = graphicsSettings;
	}

	bool HardwareMgr::CheckFrame() {
		u32 win_min = SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED;

		if (win_min) {
			return false;
		} else {
			return true;
		}
	}

	void HardwareMgr::NextFrame() {
		graphicsMgr->NextFrameImGui();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();
	}

	void HardwareMgr::RenderFrame() {
		graphicsMgr->RenderFrame();
	}

	void HardwareMgr::UpdateTexture() {
		graphicsMgr->UpdateTexture();
	}

	void HardwareMgr::ToggleFullscreen() {
		if (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) {
			SDL_SetWindowFullscreen(window, 0);
		} else {
			SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		}
	}

	void HardwareMgr::SetSwapchainSettings(bool& _present_mode_fifo, bool& _triple_buffering) {
		graphicsMgr->SetSwapchainSettings(_present_mode_fifo, _triple_buffering);
	}

	void HardwareMgr::SetFramerateTarget(const int& _target, const bool& _unlimited) {
		graphicsSettings.fpsUnlimited = _unlimited;
		graphicsSettings.framerateTarget = _target;

		if (graphicsSettings.framerateTarget > 0) {
			timePerFrame = std::chrono::milliseconds((u32)(((1.f * pow(10, 3)) / graphicsSettings.framerateTarget)));
		} else {
			timePerFrame = std::chrono::milliseconds(0);
		}
	}

	ImFont* HardwareMgr::GetFont(const int& _index) {
		return graphicsMgr->GetFont(_index);
	}

	/* *************************************************************************************************
		CONTROL BACKEND
	************************************************************************************************* */
	void HardwareMgr::ProcessEvents(bool& _running) {
		controlMgr->ProcessEvents(_running, window);
	}

	std::queue<std::pair<SDL_Keycode, bool>>& HardwareMgr::GetKeyQueue() {
		return controlMgr->GetKeyQueue();
	}

	std::queue<std::tuple<int, SDL_GameControllerButton, bool>>& HardwareMgr::GetButtonQueue() {
		return controlMgr->GetButtonQueue();
	}

	Sint32 HardwareMgr::GetScroll() {
		return controlMgr->GetScroll();
	}

	void HardwareMgr::SetMouseAlwaysVisible(const bool& _visible) {
		controlSettings.mouse_always_visible = _visible;
		currentMouseMove = 0;

		if (_visible) {
			controlMgr->SetMouseVisible(true);
		}
	}

	/* *************************************************************************************************
		AUDIO BACKEND
	************************************************************************************************* */
	void HardwareMgr::InitAudioBackend(virtual_audio_information& _virt_audio_info) {
		audioMgr->InitAudioBackend(_virt_audio_info);
	}

	void HardwareMgr::DestroyAudioBackend() {
		audioMgr->DestroyAudioBackend();
	}

	void HardwareMgr::GetAudioSettings(audio_settings _audio_settings) {
		_audio_settings = audioSettings;
	}

	void HardwareMgr::SetSamplingRate(int& _sampling_rate) {
		audioSettings.sampling_rate = _sampling_rate;
		audioMgr->SetSamplingRate(audioSettings);

		_sampling_rate = audioSettings.sampling_rate;
	}

	void HardwareMgr::SetVolume(const float& _volume, const float& _lfe) {
		audioSettings.master_volume = _volume;
		audioSettings.lfe = _volume;
		audioMgr->SetVolume(_volume, _lfe);
	}

	void HardwareMgr::SetReverb(const float& _delay, const float& _decay) {
		audioSettings.decay = _decay;
		audioSettings.delay = _delay;
		audioMgr->SetReverb(_delay, _decay);
	}

	void HardwareMgr::SetAudioChannels(const bool& _high, const bool& _low) {
		audioMgr->SetAudioChannels(_high, _low);
	}

	/* *************************************************************************************************
		NETWORK BACKEND
	************************************************************************************************* */
	void HardwareMgr::OpenNetwork(network_settings& _network_settings) {
		networkSettings = _network_settings;

		networkMgr->InitSocket(networkSettings);
	}

	bool HardwareMgr::CheckNetwork() {
		return networkMgr->CheckSocket();
	}

	void HardwareMgr::CloseNetwork() {
		networkMgr->ShutdownSocket();
	}
}