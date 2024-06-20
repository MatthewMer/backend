#pragma once

#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <vector>
#include <complex>
#include "logger.h"
#include "defs.h"

using std::swap;

namespace Backend {
	namespace Audio {
		size_t to_power_of_two(const size_t& _in);

		void window_tukey(std::complex<float>* _samples, const int& _N);
		void window_hamming(std::complex<float>* _samples, const int& _N);
		void window_blackman(std::complex<float>* _samples, const int& _N);
		void fn_window_sinc(std::vector<std::complex<float>>& _impulse_response, const int& _sampling_rate, const int& _f_cutoff, const int& _f_transtion, const bool& _high_pass);

		struct fft {
		public:
			fft() = delete;
			~fft() = delete;
			static void perform_fft(std::vector<std::complex<float>>& _samples);
			static void perform_ifft(std::vector<std::complex<float>>& _samples);

		private:
			static void fft_cooley_tukey(std::complex<float>* _data, const int& _N);
			static void ifft_cooley_tukey(std::complex<float>* _data, const int& _N);
		};

		// convolution in time domain corresponds to multiplication in frequency domain: as the convolution cancels out frequencies in the time domain that are not present (or have a very small magnitude) in the resulting signal, this logically is the same result as multiplying the frequencies in the frequency domain
		struct fir_filter {
			std::vector<std::complex<float>> frequency_response;

			std::vector<std::vector<std::complex<float>>> overlap_add;
			int cursor = 0;

			int f_transition;
			int f_cutoff;
			int sampling_rate;
			bool high_pass;
			u32 depth;
			u32 block_size;
	
			fir_filter() = default;
			fir_filter(const int& _sampling_rate, const int& _f_cutoff, const int& _f_transition, const bool& _high_pass, const u32& _block_size, const u32& _depth)
				: sampling_rate(_sampling_rate), f_cutoff(_f_cutoff), f_transition(_f_transition), high_pass(_high_pass), block_size(_block_size), depth(_depth) 
			{
				fn_window_sinc(frequency_response, _sampling_rate, _f_cutoff, _f_transition, _high_pass);
				size_t size = to_power_of_two(frequency_response.size() + block_size - 1);
				if (size < block_size * depth) {
					size = block_size * depth;
				}
				frequency_response.resize(size);
				fft::perform_fft(frequency_response);

				overlap_add.resize(depth);
				std::for_each(overlap_add.begin(), overlap_add.end(), [&size](std::vector<std::complex<float>>& _in) { _in.resize(size); });
			}

			void apply(std::vector<std::complex<float>>& _samples) {
				//window_tukey(_samples);
				for (int i = 0; i < _samples.size() / block_size; i++) {
					int offset = i * block_size;
					auto sample_block = _samples.begin() + offset;

					std::copy(sample_block, sample_block + block_size, overlap_add[cursor].begin());
					std::fill(overlap_add[cursor].begin() + block_size, overlap_add[cursor].end(), std::complex<float>());
					window_tukey(overlap_add[cursor].data(), block_size);

					fft::perform_fft(overlap_add[cursor]);
					std::transform(overlap_add[cursor].begin(), overlap_add[cursor].end(), frequency_response.begin(), overlap_add[cursor].begin(), [](std::complex<float>& lhs, std::complex<float>& rhs) { return lhs * rhs; });
					fft::perform_ifft(overlap_add[cursor]);

					std::fill(sample_block, sample_block + block_size, std::complex<float>());
					for (int j = 0; j < depth; j++) {
						auto filtered_block = overlap_add[(cursor + i) % depth].begin() + j * block_size;
						std::transform(sample_block, sample_block + block_size, filtered_block, sample_block, [](std::complex<float>& lhs, std::complex<float>& rhs) { return lhs + rhs; });
					}

					--cursor %= depth;
				}
			}
		};

		struct iir_filter {

		};
	}
}