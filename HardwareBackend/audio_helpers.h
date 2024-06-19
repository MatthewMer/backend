#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <vector>
#include <complex>
#include "logger.h"

using std::swap;

namespace Backend {
	namespace Audio {
		size_t to_power_of_two(const size_t& _in);

		void window_tukey(std::vector<std::complex<float>>& _samples);
		void window_hamming(std::vector<std::complex<float>>& _samples);
		void window_blackman(std::vector<std::complex<float>>& _samples);
		void fn_window_sinc(std::vector<std::complex<float>>& _impulse_response, const int& _sampling_rate, const int& _f_cutoff, const int& _f_transtion, const bool& _high_pass);

		struct fft {
		public:
			fft() = delete;
			~fft() = delete;
			static void perform_fft(std::vector<std::complex<float>>& _samples);
			static void perform_ifft(std::vector<std::complex<float>>& _samples);

		private:
			static std::complex<float>** fft_buffer;
			static int m;
			static int n;

			static void fft_cooley_tukey(std::complex<float>* _data_src, std::complex<float>* _data_dest, const int& _N, const int& _i, const int& _offset);
			static void ifft_cooley_tukey(std::complex<float>* _data_src, std::complex<float>* _data_dest, const int& _N, const int& _i, const int& _offset);

			inline static void resize_fft_buffer(const int& _N);
		};

		// convolution in time domain corresponds to multiplication in frequency domain: as the convolution cancels out frequencies in the time domain that are not present (or have a very small magnitude) in the resulting signal, this logically is the same result as multiplying the frequencies in the frequency domain
		struct fir_filter {
			std::vector<std::complex<float>> frequency_response;

			int f_transition;
			int f_cutoff;
			int sampling_rate;
			bool high_pass;

			fir_filter(const int& _sampling_rate, const int& _f_cutoff, const int& _f_transition, const bool& _high_pass, const int& _buffer_size)
				: sampling_rate(_sampling_rate), f_cutoff(_f_cutoff), f_transition(_f_transition), high_pass(_high_pass) {

				fn_window_sinc(frequency_response, _sampling_rate, _f_cutoff, _f_transition, _high_pass);

				size_t size;
				if (frequency_response.size() > _buffer_size) {
					size = to_power_of_two(frequency_response.size()) * 2;
				} else {
					size = to_power_of_two((size_t)_buffer_size) * 2;
				}
				frequency_response.resize(size);

				fft::perform_fft(frequency_response);
			}

			void apply(std::vector<std::complex<float>>& _samples) {
				_samples.resize(frequency_response.size());
				//window_tukey(_samples);
				fft::perform_fft(_samples);

				std::transform(_samples.begin(), _samples.end(), frequency_response.begin(), _samples.begin(), [](std::complex<float>& lhs, std::complex<float>& rhs) { return lhs * rhs; });

				fft::perform_ifft(_samples);
			}

			size_t get_size() {
				return frequency_response.size();
			}
		};

		struct iir_filter {

		};
	}
}