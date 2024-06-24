#pragma once

#include <format>

#include "AudioMgr.h"
#include "SDL_audio.h"

namespace Backend {
	namespace Audio {
		class AudioSDL : AudioMgr {
		public:
			/* *************************************************************************************************
				INIT / DEINIT AND SHUTDOWN BACKEND
			************************************************************************************************* */
			friend class AudioMgr;
			void InitAudioBackend(audio_settings& _audio_settings, const bool& _reinit) override;

			bool StartAudioBackend(virtual_audio_information& _virt_audio_info) override;
			void StopAudioBackend() override;

		protected:
			explicit AudioSDL();

		private:

			/* *************************************************************************************************
				SDL AUDIO DEVICE, SPECS PASSED TO AND RETURNED FROM SDL AUDIO
			************************************************************************************************* */
			SDL_AudioDeviceID device = {};
			SDL_AudioSpec want = {};
			SDL_AudioSpec have = {};
		};
	}
}