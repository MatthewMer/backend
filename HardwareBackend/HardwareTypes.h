#pragma once

#include "defs.h"
#include <vector>
#include <atomic>
#include <functional>
#include <complex>

namespace Backend {
	struct virtual_graphics_information {
		// drawing mode
		bool is2d = false;
		bool en2d = false;

		// data for gameboy output
		std::vector<u8>* image_data = nullptr;
		u32 lcd_width = 0;
		u32 lcd_height = 0;
		float aspect_ratio = 1.f;
	};

	struct virtual_audio_information {
		int channels = 0;
		alignas(64) std::atomic<bool> audio_running = false;
		std::function<void(std::vector<std::vector<std::complex<float>>>&, const int&, const int&)> apu_callback;

		constexpr virtual_audio_information& operator=(virtual_audio_information& _right) noexcept {
			if (this != &_right) {
				channels = _right.channels;
				audio_running.store(_right.audio_running.load());
				apu_callback = _right.apu_callback;
			}
			return *this;
		}
	};

	struct graphics_settings {
		int framerateTarget = 0;
		bool fpsUnlimited = false;
		bool tripleBuffering = false;
		bool presentModeFifo = false;
		u32 win_width = 0;
		u32 win_height = 0;
		u32 win_width_min = 0;
		u32 win_height_min = 0;
		std::string app_title = "";
		std::string icon = "";
		u32 v_major = 0;
		u32 v_minor = 0;
		u32 v_patch = 0;
		std::string font = "";
		std::string shader_folder = "";
	};

	struct audio_settings {
		int sampling_rate = 0;
		float master_volume = 0;
		float lfe = 0;
		float delay = 0;
		float decay = 0;
		int sampling_rate_max = 0;
	};

	struct control_settings {
		bool mouse_always_visible = false;
		std::string controller_db = "";
		std::string bmp_custom_cursor = "";
	};

	struct network_settings {
		std::string ipv4_address;
		int port;
	};
}