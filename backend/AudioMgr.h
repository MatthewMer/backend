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
		/* *************************************************************************************************
			ENUMS FOR SAMPLING RATE, SPEAKER SETUP (Surround 7.1, ...), USED BUFFER SIZE FOR SDL
		************************************************************************************************* */
		enum SPEAKER_SETUP {
			SOUND_MONO = 1,
			SOUND_STEREO = 2,
			SOUND_5_1 = 6,
			SOUND_7_1 = 8
		};

		enum SAMPLING_RATE {
			SOUND_22050 = 22050,
			SOUND_44100 = 44100,
			SOUND_48000 = 48000,
			SOUND_88200 = 88200,
			SOUND_96000 = 96000
		};

		enum BUFFER_SIZE {
			BUFFER_512 = 512,
			BUFFER_1024 = 1024
		};

		const std::map<const char*, std::pair<SAMPLING_RATE, BUFFER_SIZE>> SAMPLING_RATES = {
			{"22050 Hz", {SOUND_22050, BUFFER_512}},
			{"44100 Hz", {SOUND_44100, BUFFER_512}},
			{"48000 Hz", {SOUND_48000, BUFFER_512}},
			{"88200 Hz", {SOUND_88200, BUFFER_1024}},
			{"96000 Hz", {SOUND_96000, BUFFER_1024}}
		};

		/* *************************************************************************************************
			GENERIC CONSTANTS USED BY THE AUDIO BACKEND
		************************************************************************************************* */
		inline const float M_SPEED_OF_SOUND = 343.2f;	// m/s
		inline const float M_DISTANCE_EARS = 0.2f;		// m

		/* *************************************************************************************************
			ANGLES FOR THE PHYSICALLY PRESENT SPEAKERS
		************************************************************************************************* */
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

		/* *************************************************************************************************
			SAMPLES (RING BUFFER): AUDIO THREAD STORES GENERATED SAMPLES IN THE RING BUFFER AND THE
			AUDIO CALLBACK TRANSFERS THEM TO THE BUFFER USED BY SDL
		************************************************************************************************* */
		struct audio_samples {
			std::vector<float> buffer;
			int buffer_size = 0;

			int read_cursor = 0;
			int write_cursor = 0;

			std::condition_variable cond_buffer_update;
			std::mutex mut_buffer_update;
		};

		/* *************************************************************************************************
			STRUCT FOR AUDIO SETUP INFORMATION USED BY THE AUDIO THREAD AND CHANGING SETTINGS 
			DURING EXECUTION
		************************************************************************************************* */
		struct audio_information {
			SPEAKER_SETUP channels_max = SOUND_7_1;
			SAMPLING_RATE sampling_rate_max = SOUND_96000;

			SPEAKER_SETUP channels = SOUND_STEREO;
			SAMPLING_RATE sampling_rate = SOUND_44100;

			BUFFER_SIZE buff_size = BUFFER_512;

			void* device = nullptr;

			alignas(64) std::atomic<float> master_volume = 1.f;
			alignas(64) std::atomic<float> lfe = 1.f;

			alignas(64) std::atomic<float> delay = .02f;
			alignas(64) std::atomic<float> decay = .1f;

			alignas(64) std::atomic<bool> high_frequency = true;
			alignas(64) std::atomic<bool> low_frequency = true;

			alignas(64) std::atomic<bool> settings_changed = false;
		};

		/* *************************************************************************************************
			AUDIO MGR: BASE CLASS FOR AudioSDL, etc. 
		************************************************************************************************* */
		class AudioMgr {
		public:
			/* *************************************************************************************************
				GET / RESET SINGLETON INSTANCE
			************************************************************************************************* */
			static AudioMgr* getInstance();
			static void resetInstance();

			/* *************************************************************************************************
				INIT / DEINIT BACKEND, START / STOP FOR EMULATION
			************************************************************************************************* */
			virtual void InitAudioBackend(audio_settings& _audio_settings, const bool& _reinit) = 0;

			virtual bool StartAudioBackend(virtual_audio_information& _virt_audio_info) = 0;
			virtual void StopAudioBackend() = 0;

			/* *************************************************************************************************
				SETTERS FOR AUDIO THREAD
			************************************************************************************************* */
			void SetSamplingRate(audio_settings& _audio_settings);

			void SetVolume(const float& _volume, const float& _lfe);
			void SetReverb(const float& _delay, const float& _decay);

			void SetAudioChannels(const bool& _high, const bool& _low);

			/* *************************************************************************************************
				CLONE / ASSIGN PROTECTION
			************************************************************************************************* */
			AudioMgr(AudioMgr const&) = delete;
			AudioMgr(AudioMgr&&) = delete;
			AudioMgr& operator=(AudioMgr const&) = delete;
			AudioMgr& operator=(AudioMgr&&) = delete;

		protected:
			/* *************************************************************************************************
				CONSTRUCTOR
			************************************************************************************************* */
			explicit AudioMgr();
			~AudioMgr() = default;

			/* *************************************************************************************************
				MEMBERS: NAME OF DEVICE, SAMPLES STRUCT, THE AUDIO THREAD FOR SAMPLE GENERATION
				FOR EMULATION
			************************************************************************************************* */
			std::string name = "";
			audio_samples audioSamples;
			std::thread audioThread;

			/* *************************************************************************************************
				INFO STRUCTS FOR AUDIO THREAD
			************************************************************************************************* */
			audio_information audioInfo = {};
			virtual_audio_information virtAudioInfo = {};

		private:
			/* *************************************************************************************************
				THE AudioMgr INSTANCE
			************************************************************************************************* */
			static AudioMgr* instance;
		};
	}
}