#include "pch.h"
#include "framework.h"

#include "AudioSDL.h"
#include "format"
#include "AudioMgr.h"
#include "logger.h"
#include "audio_helpers.h"
#include "SDL_audio.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <glm.hpp>
#include <format>

using namespace std;

namespace Backend {
	namespace Audio {
		/* *************************************************************************************************
			CONSTRUCTOR
		************************************************************************************************* */
		AudioSDL::AudioSDL() : AudioMgr() {
			SDL_AudioSpec dev_props;
			char* c_name;
			SDL_GetDefaultAudioInfo(&c_name, &dev_props, 0);
			name = std::string(c_name);

			if (audioInfo.sampling_rate_max > dev_props.freq) {
				audioInfo.sampling_rate_max = (SAMPLING_RATE)dev_props.freq;
				audioInfo.sampling_rate = (SAMPLING_RATE)dev_props.freq;
			} else {
				audioInfo.sampling_rate = audioInfo.sampling_rate_max;
			}

			if (audioInfo.channels_max > dev_props.channels) {
				audioInfo.channels_max = (SPEAKER_SETUP)dev_props.channels;
				audioInfo.channels = (SPEAKER_SETUP)dev_props.channels;
			} else {
				audioInfo.channels = audioInfo.channels_max;
			}

			LOG_INFO("[SDL] ", name, " supports: ", std::format("{:d} channels @ {:d}Hz", dev_props.channels, dev_props.freq));
		}

		/* *************************************************************************************************
			CALLBACK AND THREAD FUNCTIONS FOR THE AUDIO THREAD AND SDL BACKEND
			(GENERATE NEW SAMPLES STORED IN THE RING BUFFER, REQUEST NEW SAMPLES FOR THE RING BUFFER)
		************************************************************************************************* */
		void audio_callback(void* userdata, u8* _device_buffer, int _length);
		void audio_thread(audio_information* _audio_info, virtual_audio_information* _virt_audio_info, audio_samples* _samples);

		/* *************************************************************************************************
			INIT BASIC AUDIO BACKEND FUNCTIONALITY
		************************************************************************************************* */
		void AudioSDL::InitAudioBackend(audio_settings& _audio_settings, const bool& _reinit) {
			bool _reinit_backend = virtAudioInfo.audio_running.load();

			if (_reinit) {
				if (_reinit_backend) {
					StopAudioBackend();
				}
				SDL_CloseAudioDevice(device);
			}

			SDL_zero(want);
			SDL_zero(have);

			if (_audio_settings.sampling_rate > audioInfo.sampling_rate_max) {
				_audio_settings.sampling_rate = audioInfo.sampling_rate_max;
			}

			Uint16 buff_size = 512;
			for (const auto& [key, val] : SAMPLING_RATES) {
				if (val.first == _audio_settings.sampling_rate) {
					buff_size = (Uint16)val.second;
				}
			}

			want.freq = _audio_settings.sampling_rate;
			want.format = AUDIO_F32;
			want.channels = (u8)audioInfo.channels;
			want.samples = buff_size;						// buffer size per channel
			want.callback = audio_callback;
			want.userdata = &audioSamples;
			device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);

			// audio info (settings)
			audioInfo.sampling_rate = (SAMPLING_RATE)have.freq;
			audioInfo.channels = (SPEAKER_SETUP)have.channels;
			audioInfo.buff_size = (BUFFER_SIZE)have.samples;

			_audio_settings.sampling_rate = have.freq;
			_audio_settings.sampling_rate_max = audioInfo.sampling_rate_max;

			audioInfo.master_volume.store(_audio_settings.master_volume);
			audioInfo.lfe_volume.store(_audio_settings.lfe_volume);
			audioInfo.base_volume.store(_audio_settings.base_volume);
			audioInfo.decay.store(_audio_settings.decay);
			audioInfo.delay.store(_audio_settings.delay);
			audioInfo.hf_channel_output.store(_audio_settings.hf_output_enable);
			audioInfo.lfe_channel_output.store(_audio_settings.lfe_output_enable);
			audioInfo.dist_low_pass_enable.store(_audio_settings.dist_low_pass_enable);
			audioInfo.lfe_low_pass_enable.store(_audio_settings.lfe_low_pass_enable);
			audioInfo.settings_changed.store(false);

			// audio samples (audio api data)
			int format_size = SDL_AUDIO_BITSIZE(have.format) / 8;
			bool is_float = SDL_AUDIO_ISFLOAT(have.format);
			if (format_size != sizeof(float) || !is_float) {
				LOG_ERROR("[SDL] audio format not supported");
				SDL_CloseAudioDevice(device);
				return;
			}
			audioSamples.buffer = std::vector<float>(audioInfo.buff_size * audioInfo.channels * 4, .0f);
			audioSamples.buffer_size = (int)audioSamples.buffer.size() * sizeof(float);
			audioSamples.write_cursor = 0;
			audioSamples.read_cursor = (audioSamples.write_cursor - (have.samples * have.channels) + (int)audioSamples.buffer.size()) % (int)audioSamples.buffer.size();

			// finish audio data
			audioInfo.device = (void*)&device;

			SDL_PauseAudioDevice(device, 0);
			LOG_INFO("[SDL] ", name, " set: ", std::format("{:d} channels @ {:d}Hz", (int)audioInfo.channels, (int)audioInfo.sampling_rate));

			if (_reinit && _reinit_backend) {
				StartAudioBackend(virtAudioInfo);
			}
		}

		/* *************************************************************************************************
			START / STOP AUDIO BACKEND FOR EMULATION (GENERATE NEW SAMPLES FOR AUDIO BUFFER)
		************************************************************************************************* */
		bool AudioSDL::StartAudioBackend(virtual_audio_information& _virt_audio_info) {
			virtAudioInfo = _virt_audio_info;
			virtAudioInfo.audio_running.store(true);

			audioThread = thread(audio_thread, &audioInfo, &virtAudioInfo, &audioSamples);
			if (!audioThread.joinable()) {
				return false;
			} else {
				LOG_INFO("[SDL] audio backend initialized");
				return true;
			}
		}

		void AudioSDL::StopAudioBackend() {
			virtAudioInfo.audio_running.store(false);
			audioSamples.cond_buffer_update.notify_one();
			if (audioThread.joinable()) {
				audioThread.join();
			}
			for (auto& n : audioSamples.buffer) {
				n = .0f;
			}
			LOG_INFO("[SDL] audio backend stopped");
		}

		/* *************************************************************************************************
			DIFFERENT BUFFERS AND ALGORITHMS FOR CREATING DIFFERENT AUDIO EFFECTS
		************************************************************************************************* */

		/* *************************************************************************************************
			SIMULATES THE DIFFERENCE IN TIME A SIGNAL NEEDS TO TRAVEL TO THE RIGHT AND LEFT EAR
			(not finished)
		************************************************************************************************* */
		struct delay_buffer {
			std::vector<std::vector<float>> buffer;
			int cursor = 0;

			glm::vec2 left = glm::vec2(-M_DISTANCE_EARS / 2, 0);
			glm::vec2 right = glm::vec2(M_DISTANCE_EARS / 2, 0);

			int sampling_rate;

			delay_buffer() = delete;
			delay_buffer(const int& _sampling_rate, const int& _num_buffers) : sampling_rate(_sampling_rate) {
				buffer.assign(_num_buffers, {});
				for (auto& n : buffer) {
					n.assign((size_t)((M_DISTANCE_EARS / M_SPEED_OF_SOUND) * _sampling_rate), .0f);
				}
			}

			void filter(std::vector<std::vector<std::complex<float>>> _samples) {

			}

			void insert(const float& _sample, const float& _angle, const float& _distance) {
				glm::vec2 e = glm::vec2(sin(_angle), cos(_angle));
				glm::vec2 pos = e * _distance;
				float diff = abs(glm::length(pos + left) - glm::length(pos + right));

				int offset = (int)((diff / M_SPEED_OF_SOUND) * sampling_rate);

				// insert samples delayed with respect to direction into buffer and take samples for output from there instead 
				// to take slight time differences between left and right ear into account which brain uses to determine direction
			}
		};

		/* *************************************************************************************************
			REPEATS SAMPLE DELAYED WHICH CREATES AN ECHO -> USED FOR AUDIO DEPTH
		************************************************************************************************* */
		struct reverb_buffer {
			std::vector<float> buffer;
			float decay = .0f;
			int cursor = 0;

			reverb_buffer() = default;
			reverb_buffer(const int& _delay, const float& _decay) : decay(_decay) {
				if (_delay > 0) {
					buffer.assign(_delay, .0f);
				} else {
					buffer.assign(1, .0f);
				}
			}

			void add(const float& _sample) {
				buffer[cursor] += _sample;
			}

			float next() {
				buffer[cursor] *= decay;
				++cursor %= buffer.size();
				return buffer[cursor];
			}
		};

		/* *************************************************************************************************
			SPEAKERS STRUCT THAT CONTAINS THE FUNCTIONALITY TO APPLY AUDIO EFFECTS TO A
			GIVEN SET OF SAMPLES AND OUTPUTTING THEM TO THE RING BUFFER FOR THE SDL CALLBACK
		************************************************************************************************* */
		struct speakers {
			typedef void (speakers::* speaker_function)(float*, const float&, const float&, const float&);
			speaker_function func;

			reverb_buffer r_buffer;
			//delay_buffer d_buffer;

			std::vector<std::complex<float>> dist_buffer;
			fir_filter low_pass_distance;

			std::vector<std::vector<std::complex<float>>> lfe_buffer;
			fir_filter low_pass_lfe;

			std::vector<float> virt_angles;
			std::vector<std::complex<float>> virt_samples;

			SDL_AudioDeviceID* device;

			audio_information* audio_info;
			virtual_audio_information* virt_audio_info;
			audio_samples* samples;

			int buff_size;

			float decay;
			float delay;

			int sampling_rate;

			float base_volume;
			float lfe_volume;
			float master_volume;

			bool hf_channel_output = true;
			bool lfe_channel_output = true;

			bool lfe_low_pass_enable = false;
			bool dist_low_pass_enable = false;

			int channels;
			int virt_channels;

			speakers() = delete;
			speakers(audio_information* _audio_info, virtual_audio_information* _virt_audio_info, audio_samples* _samples) {
				audio_info = _audio_info;
				virt_audio_info = _virt_audio_info;
				samples = _samples;

				device = (SDL_AudioDeviceID*)audio_info->device;

				decay = _audio_info->decay.load();
				delay = _audio_info->delay.load();
				sampling_rate = _audio_info->sampling_rate;
				lfe_volume = _audio_info->lfe_volume.load();
				master_volume = _audio_info->master_volume.load();
				buff_size = _audio_info->buff_size;
				hf_channel_output = _audio_info->hf_channel_output.load();
				lfe_channel_output = _audio_info->lfe_channel_output.load();
				channels = _audio_info->channels;
				base_volume = _audio_info->base_volume.load();
				dist_low_pass_enable = _audio_info->dist_low_pass_enable.load();
				lfe_low_pass_enable = _audio_info->lfe_low_pass_enable.load();

				virt_channels = _virt_audio_info->channels;

				r_buffer = reverb_buffer((int)(delay * sampling_rate), decay);

				low_pass_distance = fir_filter(sampling_rate, 3000, TRANSITION_BANDWITH::BW_750, false, buff_size);
				dist_buffer = std::vector<std::complex<float>>(buff_size);

				low_pass_lfe = fir_filter(sampling_rate, 100, TRANSITION_BANDWITH::BW_750, false, buff_size);
				lfe_buffer = std::vector<std::vector<std::complex<float>>>(virt_channels);
				for (auto& n : lfe_buffer) {
					n = std::vector<std::complex<float>>(buff_size);
				}

				// determine required buffer sizes
				virt_samples = std::vector<std::complex<float>>(virt_channels * buff_size);

				switch (audio_info->channels) {
				case SOUND_7_1:
					func = &speakers::samples_7_1_surround;
					break;
				case SOUND_5_1:
					func = &speakers::samples_5_1_surround;
					break;
				case SOUND_STEREO:
					func = &speakers::samples_stereo;
					break;
				case SOUND_MONO:
					func = &speakers::samples_mono;
					break;
				default:
					func = nullptr;
					LOG_ERROR("[SDL] speaker configuration currently not supported");
					break;
				}

				float a;
				if (channels == SOUND_7_1 || channels == SOUND_5_1) {
					a = 22.5f;
				} else {
					a = 45.f;
				}

				float step = (float)((360.f - (2 * a)) / (virt_channels - 1));

				for (int i = 0; i < virt_channels; i++) {
					virt_angles.push_back(a * (float)(M_PI / 180.f));
					a += step;
				}
			}

			/* *************************************************************************************************
				GENERATE NEW SAMPLES REQUESTED BY THE SDL CALLBACK
			************************************************************************************************* */
			void process() {
				if (audio_info->settings_changed.load()) {
					master_volume = audio_info->master_volume.load();
					lfe_volume = audio_info->lfe_volume.load();
					reset_reverb(audio_info->delay.load(), audio_info->decay.load());
					hf_channel_output = audio_info->hf_channel_output.load();
					lfe_channel_output = audio_info->lfe_channel_output.load();
					base_volume = audio_info->base_volume.load();
					dist_low_pass_enable = audio_info->dist_low_pass_enable.load();
					lfe_low_pass_enable = audio_info->lfe_low_pass_enable.load();
					audio_info->settings_changed.store(false);
				}

				SDL_LockAudioDevice(*device);

				int reg_1_size, reg_2_size;
				if (samples->read_cursor <= samples->write_cursor) {
					reg_1_size = (int)samples->buffer.size() - samples->write_cursor;
					reg_2_size = samples->read_cursor;
				} else if (samples->read_cursor > samples->write_cursor) {
					reg_1_size = samples->read_cursor - samples->write_cursor;
					reg_2_size = 0;
				}

				if (reg_1_size || reg_2_size) {
					// get samples from APU
					int reg_1_samples = reg_1_size / channels;
					int reg_2_samples = reg_2_size / channels;
					int num_samples = reg_1_samples + reg_2_samples;

					virt_samples.assign(num_samples * virt_channels, std::complex<float>());
					virt_audio_info->apu_callback(virt_samples, num_samples, sampling_rate);

					// distance (reverberation)
					if (dist_low_pass_enable) {
						dist_buffer.assign(num_samples, std::complex<float>());
						for (int i = 0, j = 0; i < num_samples; j++) {
							if (j == virt_channels) { j = 0; i++; }
							dist_buffer[i] += virt_samples[i * virt_channels + j];
						}
						low_pass_distance.apply(dist_buffer);
					}

					// lfe lowpass
					if (lfe_low_pass_enable) {
						for (auto& n : lfe_buffer) {
							n.resize(num_samples);
						}

						for (int i = 0; i < virt_channels; i++) {
							for (int j = 0; j < num_samples; j++) {
								lfe_buffer[i][j] = virt_samples[j * virt_channels + i];
							}
							low_pass_lfe.apply(lfe_buffer[i]);
						}
					}

					int offset = samples->write_cursor;
					for (int i = 0; i < num_samples; i++) {
						float reverb = dist_low_pass_enable ? r_buffer.next() : .0f;

						for (int j = 0; j < virt_channels; j++) {
							float sample = virt_samples[i * virt_channels + j].real() * base_volume + reverb;
							float hf_sample = (hf_channel_output ? sample * master_volume : .0f);
							float lfe_sample = (lfe_channel_output ? (lfe_low_pass_enable ? (lfe_buffer[j][i].real() * base_volume + reverb) : sample) * lfe_volume * master_volume : .0f);
							(this->*func)(&samples->buffer[offset], hf_sample, virt_angles[j], lfe_sample);
						}

						r_buffer.add(dist_buffer[i].real());

						(offset += channels) %= samples->buffer.size();
					}
					samples->write_cursor = (samples->write_cursor + reg_1_size + reg_2_size) % (int)samples->buffer.size();		// update cursor in bytes (due to the callback working in bytes instead of floats because of SDL)
				}
				SDL_UnlockAudioDevice(*device);
			}

			/* *************************************************************************************************
				SETTERS FOR DIFFERENT BUFFERS USED DURING SAMPLE GENERATION
			************************************************************************************************* */
			void reset_reverb(const float& _delay, const float& _decay) {
				r_buffer = reverb_buffer((int)(_delay * sampling_rate), _decay);
			}

			const float x = .5f;
			const float D = 1.2f;		// gain
			const float a = 2.f;

			/* *************************************************************************************************
				DIFFERENT FUNCTIONS FOR SAMPLE OUTPUT DEPENDING ON THE PHYSICAL SPEAKER SETUP
			************************************************************************************************* */
			// using angles in rad -> translate samples to speaker depending the angle (direction)
			// -> https://www.desmos.com/calculator/vpkgagyrhz?lang=de
			float calc_sample(const float& _sample, const float& _sample_angle, const float& _speaker_angle) {
				return tanh(D * _sample) * exp(a * .5f * cos(_sample_angle - _speaker_angle) - .5f);
			}

			void samples_7_1_surround(float* _buffer, const float& _sample, const float& _angle, const float& _lfe) {
				_buffer[0] += calc_sample(_sample, _angle, SOUND_7_1_ANGLES[0]);	// front-left
				_buffer[1] += calc_sample(_sample, _angle, SOUND_7_1_ANGLES[1]);	// front-right
				_buffer[2] += calc_sample(_sample, _angle, SOUND_7_1_ANGLES[2]);	// centre
				_buffer[4] += calc_sample(_sample, _angle, SOUND_7_1_ANGLES[4]);	// rear-left
				_buffer[5] += calc_sample(_sample, _angle, SOUND_7_1_ANGLES[5]);	// rear-right
				_buffer[6] += calc_sample(_sample, _angle, SOUND_7_1_ANGLES[6]);	// centre-left
				_buffer[7] += calc_sample(_sample, _angle, SOUND_7_1_ANGLES[7]);	// centre-right

				_buffer[3] += _lfe;
			}

			void samples_5_1_surround(float* _buffer, const float& _sample, const float& _angle, const float& _lfe) {
				_buffer[0] += calc_sample(_sample, _angle, SOUND_5_1_ANGLES[0]);
				_buffer[1] += calc_sample(_sample, _angle, SOUND_5_1_ANGLES[1]);
				_buffer[2] += calc_sample(_sample, _angle, SOUND_5_1_ANGLES[2]);
				_buffer[4] += calc_sample(_sample, _angle, SOUND_5_1_ANGLES[4]);
				_buffer[5] += calc_sample(_sample, _angle, SOUND_5_1_ANGLES[5]);

				_buffer[3] += _lfe;
			}

			void samples_stereo(float* _buffer, const float& _sample, const float& _angle, const float& _lfe) {
				_buffer[0] += calc_sample(_sample, _angle, SOUND_STEREO_ANGLES[0]) + _lfe;
				_buffer[1] += calc_sample(_sample, _angle, SOUND_STEREO_ANGLES[1]) + _lfe;
			}

			void samples_mono(float* _buffer, const float& _sample, const float& _angle, const float& _lfe) {
				_buffer[0] += calc_sample(_sample, _angle, SOUND_STEREO_ANGLES[0]) + _lfe;
				_buffer[0] += calc_sample(_sample, _angle, SOUND_STEREO_ANGLES[1]) + _lfe;
			}
		};

		/* *************************************************************************************************
			CALLBACK AND THREAD FUNCTION FOR SDL CALLBACK AND SAMPLE GENERATION ON REQUEST OF SDL
			_user_data: struct passed to audiospec
			_device_buffer: the audio buffer snippet that needs to be filled
			_length: length of this buffer snippet
		************************************************************************************************* */
		void audio_callback(void* _user_data, u8* _device_buffer, int _length) {
			audio_samples* samples = (audio_samples*)_user_data;

			float* reg_2 = samples->buffer.data();
			float* reg_1 = reg_2 + samples->read_cursor;

			int b_read_cursor = samples->read_cursor * sizeof(float);

			int reg_1_size, reg_2_size;
			if (b_read_cursor + _length > samples->buffer_size) {
				reg_1_size = samples->buffer_size - b_read_cursor;
				reg_2_size = _length - reg_1_size;
			} else {
				reg_1_size = _length;
				reg_2_size = 0;
			}

			SDL_memcpy(_device_buffer, reg_1, reg_1_size);
			SDL_memcpy(_device_buffer + reg_1_size, reg_2, reg_2_size);

			memset(reg_1, 0, reg_1_size);
			memset(reg_2, 0, reg_2_size);

			samples->read_cursor = ((b_read_cursor + _length) % samples->buffer_size) / sizeof(float);

			samples->cond_buffer_update.notify_one();
		}

		void audio_thread(audio_information* _audio_info, virtual_audio_information* _virt_audio_info, audio_samples* _samples) {
			speakers sp = speakers(
				_audio_info, _virt_audio_info, _samples
			);

			while (_virt_audio_info->audio_running.load()) {
				std::unique_lock<mutex> lock_buffer(_samples->mut_buffer_update);
				_samples->cond_buffer_update.wait(lock_buffer);

				sp.process();
			}
		}
	}
}