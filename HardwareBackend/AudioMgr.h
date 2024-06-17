#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include "HardwareTypes.h"
#include <thread>
#include <mutex>
#include <complex>
#include <map>

namespace Backend {
	namespace Audio {
#define SOUND_MONO                  1
#define SOUND_STEREO                2
#define SOUND_5_1                   6
#define SOUND_7_1                   8

#define M_SPEED_OF_SOUND            343.2f  // m/s
#define M_DISTANCE_EARS             0.2f    // m

		const std::map<const char*, std::pair<int, int>> SAMPLING_RATES = {
			{"22050 Hz", {22050, 512}},
			{"44100 Hz", {44100, 512}},
			{"48000 Hz", {48000, 512}},
			{"88200 Hz", {88200, 1024}},
			{"96000 Hz", {96000, 1024}}
		};

		const float SOUND_7_1_ANGLES[8] = {
			337.5f * (float)(M_PI / 180.f),               // front-left
			22.5f * (float)(M_PI / 180.f),                // front-right
			.0f * (float)(M_PI / 180.f),                  // centre
			.0f * (float)(M_PI / 180.f),                  // low frequency (not needed)
			220.f * (float)(M_PI / 180.f),                // rear-left
			140.f * (float)(M_PI / 180.f),                // rear-right
			275.f * (float)(M_PI / 180.f),                // centre-left
			85.f * (float)(M_PI / 180.f)                  // centre-right
		};

		const float SOUND_5_1_ANGLES[6] = {
			337.5f * (float)(M_PI / 180.f),               // front-left
			22.5f * (float)(M_PI / 180.f),                // front-right
			.0f * (float)(M_PI / 180.f),                  // centre
			.0f * (float)(M_PI / 180.f),                  // low frequency (not needed)
			220.f * (float)(M_PI / 180.f),                // rear-left
			140.f * (float)(M_PI / 180.f)                 // rear-right
		};

		const float SOUND_STEREO_ANGLES[2] = {
			270.f * (float)(M_PI / 180.f),                // left
			90.f * (float)(M_PI / 180.f)                  // right
		};

		struct audio_samples {
			std::vector<float> buffer;
			int buffer_size = 0;

			int read_cursor = 0;
			int write_cursor = 0;

			std::condition_variable notifyBufferUpdate;
			std::mutex mutBufferUpdate;
		};

		struct audio_information {
			int channels_max = 0;
			int sampling_rate_max = 0;

			int channels = 0;
			int sampling_rate = 0;

			int buff_size = 0;

			void* device = nullptr;

			alignas(64) std::atomic<float> master_volume = 1.f;
			alignas(64) std::atomic<float> lfe = 1.f;
			alignas(64) std::atomic<bool> volume_changed = false;

			alignas(64) std::atomic<float> delay = .02f;
			alignas(64) std::atomic<float> decay = .1f;
			alignas(64) std::atomic<bool> reload_reverb = false;
		};

		class AudioMgr {
		public:
			// get/reset instance
			static AudioMgr* getInstance();
			static void resetInstance();

			virtual void InitAudio(audio_settings& _audio_settings, const bool& _reinit) = 0;

			virtual bool InitAudioBackend(virtual_audio_information& _virt_audio_info) = 0;
			virtual void DestroyAudioBackend() = 0;

			void SetSamplingRate(audio_settings& _audio_settings);

			void SetVolume(const float& _volume, const float& _lfe);
			void SetReverb(const float& _delay, const float& _decay);

			// clone/assign protection
			AudioMgr(AudioMgr const&) = delete;
			AudioMgr(AudioMgr&&) = delete;
			AudioMgr& operator=(AudioMgr const&) = delete;
			AudioMgr& operator=(AudioMgr&&) = delete;

		protected:
			// constructor
			explicit AudioMgr() {
				int sampling_rate_max = 0;
				for (const auto& [key, val] : SAMPLING_RATES) {
					if (val.first > sampling_rate_max) { sampling_rate_max = val.first; }
				}

				audioInfo.channels_max = SOUND_7_1;
				audioInfo.sampling_rate_max = sampling_rate_max;
				audioInfo.channels = 0;
				audioInfo.sampling_rate = 0;
			}
			~AudioMgr() = default;

			std::string name = "";
			audio_samples audioSamples;
			std::thread audioThread;

			audio_information audioInfo = {};
			virtual_audio_information virtAudioInfo = {};

		private:
			static AudioMgr* instance;
		};
	}
}