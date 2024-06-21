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
			u32 B;
			u32 L;
			u32 N;
	
			fir_filter() = default;
			fir_filter(const int& _sampling_rate, const int& _f_cutoff, const int& _f_transition, const bool& _high_pass, const u32& _block_size)
				: sampling_rate(_sampling_rate), f_cutoff(_f_cutoff), f_transition(_f_transition), high_pass(_high_pass), B(_block_size)
			{
				fn_window_sinc(frequency_response, _sampling_rate, _f_cutoff, _f_transition, _high_pass);
				L = frequency_response.size();
				N = to_power_of_two(L + B - 1);
				frequency_response.resize(N);
				fft::perform_fft(frequency_response);

				depth = (int)std::ceil((float)N / B);
				overlap_add.resize(depth);
				size_t size = N;
				std::for_each(overlap_add.begin(), overlap_add.end(), [&size](std::vector<std::complex<float>>& _in) { _in.resize(size); });
			}

			void apply(std::vector<std::complex<float>>& _X) {
				for (int i = 0; i < _X.size() / B; i++) {
					auto B_ = _X.begin() + i * B;
					auto& N_cur = overlap_add[cursor];

					std::copy(B_, B_ + B, N_cur.begin());
					std::fill(N_cur.begin() + B, N_cur.end(), std::complex<float>());
					window_tukey(N_cur.data(), B);

					fft::perform_fft(N_cur);
					std::transform(N_cur.begin(), N_cur.end(), frequency_response.begin(), N_cur.begin(), [](std::complex<float>& lhs, std::complex<float>& rhs) { return lhs * rhs; });
					fft::perform_ifft(N_cur);

					std::copy(B_, B_ + B, N_cur.begin());
					for (int j = 1; j < depth; j++) {
						auto& N_prev = overlap_add[(cursor + j) % depth];

						int offset = B;
						auto L_prev = N_prev.begin() + (j - 1) * B;
						if (std::distance(L_prev, N_prev.end()) < B) {
							offset = std::distance(L_prev, N_prev.end());
						}
						std::transform(L_prev, L_prev + offset, B_, B_, [](std::complex<float>& lhs, std::complex<float>& rhs) { return lhs + rhs; });
					}

					--cursor %= depth;
				}
			}
		};

		struct iir_filter {

		};
	}
}