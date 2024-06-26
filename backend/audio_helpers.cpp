﻿#include "pch.h"
#include "framework.h"

#include "audio_helpers.h"

namespace Backend {
	namespace Audio {
		/* *************************************************************************************************
			RETURNS THE NEXT POWER OF 2, IF IT WAS ALREADY A POWER OF 2 THE SAME VALUE IS RETURNED
		************************************************************************************************* */
		int to_power_of_two(const int& _in) {
			return (int)pow(2, (int)std::ceil(log2(_in)));
		}

		/* *************************************************************************************************
			SORT FUNCTIONS USED FOR FFT:
			SAMPLES AT EVEN INDICES PLACED IN THE RANGE 0 TO N/2-1
			SAMPLES AT ODD INDICES PLACED IN THE RANGE N/2 TO N-1
		************************************************************************************************* */
		void sort(std::complex<float>* _data, int _N) {
			if (_N == 2) { return; }
			for (int n = 1; n < _N / 2; n += 2) {
				std::swap(_data[n], _data[n + _N / 2 - 1]);
			}
			sort(_data, _N / 2);
			sort(_data + _N / 2, _N / 2);
		}

		void revert(std::complex<float>* _data, int _N) {
			if (_N == 2) { return; }
			revert(_data, _N / 2);
			revert(_data + _N / 2, _N / 2);
			for (int n = 1; n < _N / 2; n += 2) {
				std::swap(_data[n], _data[n + _N / 2 - 1]);
			}
		}

		/*
		FFT algorithm -> transform signal from time domain into frequency domain (application of fourier analysis)
			FT: X(f) = integral(t=-infinity,infinity) x(t) * e^(-i*2*pi*f*t) dt, where x(t) is the signal and e^(-i*2*pi*f*t) the complex exponential (polar form: e^(j*phi),
			which can be directly translated into the trigonometric form with eulers formula: e^(j*phi) = cos(phi) + j * sin(phi)) which contains the cosine component in the real part
			and the sine component in the imaginary part. The complex exponential can be viewed as a spiral coming out of the imaginary plane, rotating counter clockwise around the
			t-axis with the radius omega*t (omega=2*pi*f) or amplitude, starting at cos(0)+i*sin(0) = 1. When plotting this spiral to the imaginary axis you end up with the sine component,
			when plotting it to the real axis you get the cosine component. These two components resemble each sinusoid in the frequency domain the input signal is made off. The convolution
			of these two components (amplitude) with the signal itself (multiply and integrate, as described by the formula above) delivers the area enclosed by the resulting signal, which also resembles
			its "impact". Using Pythagoras on these two values (both components resemble a right triangle in the complex plane) you get the magnitude of the sinusoid, using the 4 quadrant inverse tangent
			(atan2(opposite side / adjacent side)) you get its phase. In the FFT this "spiral" rotates clockwise ((e^(i*2*pi*f*t))^-1 = e^(-i*2*pi*f*t)), which has the effekt of morroring the sine component
			on the t-axis (rotating the circle in the complex plane clockwise). The point of intersection of this spiral with the complex plane is therefore cos(0)-i*sin(0) and when adding this
			complex exponent to the signal (which has its imaginary part set to i*sin(omega*t)=i0) is where the phase shift along the t-axis happens.

			FFT (which is an optimized version of DFT) splits the samples in half until each one contains 2 values and processes recursively the DFT for each half. The regular FT is actually impossible
			to compute because it tests an infinite amount of waves (represented by the resulting complex numbers), starting at 0Hz (DC), to the 1st harmonic, the 2nd harmonic
			up to the Nyquist Frequency (Sampling Rate / 2, at least 2 samples  per period are required to reconstruct a wave).
			The Input for Cooley-Tukey FFT has to be a power of 2 and for the FFT in general contain samples of one full period of the Signal. There are more optimized versions that
			bypass the requirement for n^2 samples.
			FFT (optimized DFT):
			X[k] = Σ (n=0 bis N-1) x[n] * exp(-i*2*pi * k * n / N) = Σ (n=0 bis N-1) x[n] * (cos(2*pi*k*n/N)-i*sin(2*pi*k*n/N))
		*/

		/* *************************************************************************************************
			PERFORM A RADIX-2 (I)FFT (REQUIRES SAMPLE COUNT POWER OF 2) FOR A GIVEN DISCRETE SIGNAL
		************************************************************************************************* */
		// FFT based on Cooley-Tukey
		// returns DFT (size of N) with the phases, magnitudes and frequencies in cartesian form (e.g. 3.5+2.6i)
		// example in pseudo code: https://en.wikipedia.org/wiki/Cooley%E2%80%93Tukey_FFT_algorithm
		void fft::perform_fft(std::vector<std::complex<float>>& _samples) {
			fft_cooley_tukey(_samples.data(), (int)_samples.size());
		}

		void fft::fft_cooley_tukey(std::complex<float>* _data, const int& _N) {
			// only one sample
			if (_N == 1) {
				return;
			}

			// split in 2 arrays, one for all elements where n is odd and one where n is even
			sort(_data, _N);

			// recursively call fft for all stages (with orders power of 2)
			fft_cooley_tukey(_data, _N / 2);
			fft_cooley_tukey(_data + _N / 2, _N / 2);

			// twiddel factors ( e^(-i*2*pi*k/N) , where k = index and N = order
			// used for shifting the signal
			for (int k = 0; k < _N / 2; k++) {
				float ang = (float)(2 * M_PI * k / _N);
				std::complex<float> p = _data[k];
				std::complex<float> q = std::polar(1.f, (float)(-2.f * M_PI * (float)k / _N)) * _data[k + _N / 2];
				_data[k] = p + q;
				_data[k + _N / 2] = p - q;
			}
		}

		// inverse FFT: convert samples from frequency domain back to time domain (signal)
		// Formulas can be found here: https://www.dsprelated.com/showarticle/800.php
		// x[n] = 1/N * sum(m=0 to N-1) (X[m]*(cos(2*PI*m*n/N)+i*sin(2*PI*m*n/N)))		-> discarded normalization factor 1/N
		void fft::perform_ifft(std::vector<std::complex<float>>& _samples) {
			ifft_cooley_tukey(_samples.data(), (int)_samples.size());

			float divisor = (float)_samples.size();
			std::transform(_samples.begin(), _samples.end(), _samples.begin(), [&divisor](std::complex<float>& val) { return val /= std::complex<float>(divisor, .0f); });
		}

		void fft::ifft_cooley_tukey(std::complex<float>* _data, const int& _N) {
			// only one sample
			if (_N == 1) {
				return;
			}

			// split in 2 arrays, one for all elements where n is odd and one where n is even
			sort(_data, _N);

			// recursively call fft for all stages (with orders power of 2)
			ifft_cooley_tukey(_data, _N / 2);
			ifft_cooley_tukey(_data + _N / 2, _N / 2);

			// twiddel factors ( e^(-i*2*pi*k/N) , where k = index and N = order
			// used for shifting the signal
			for (int k = 0; k < _N / 2; k++) {
				float ang = (float)(2 * M_PI * k / _N);
				std::complex<float> p = _data[k];
				std::complex<float> q = std::polar(1.f, (float)(2.f * M_PI * (float)k / _N)) * _data[k + _N / 2];
				_data[k] = p + q;
				_data[k + _N / 2] = p - q;
			}
		}

		/* *************************************************************************************************
			WINDOWING FUNCTIONS FOR MAKING SIGNAL "PERIODIC" FOR A GIVEN SEQUENCE
			(BECOMING 0 AT THE BEGINNING AND THE END)
		************************************************************************************************* */
		const float alpha = .0f;

		// necessary, as it is impossible to assure that the sampled signal consists of exactly n periods (where n is an integer) and is continuous (doesn't matter in our application)
		// could come in handy for something like a N64 where audio files (sort of) are played
		// resources:
		// https://en.wikipedia.org/wiki/Window_function
		void window_tukey(std::complex<float>* _samples, const int& _N) {
			int N = _N - 1;
			int t_low = (int)(((alpha * N) / 2) + .5f);				// + .5f for rounding
			int t_high = (int)((N - (alpha * N) / 2) + .5f);

			for (int n = 0; n < t_low; n++) {
				_samples[n] *= .5f - .5f * (float)cos(2 * M_PI * n / (alpha * N));
			}
			for (int n = t_high + 1; n < _N; n++) {
				_samples[n] *= .5f - .5f * (float)cos(2 * M_PI * (N - n) / (alpha * N));
			}
		}

		void window_hamming(std::complex<float>* _samples, const int& _N) {
			int N = _N - 1;
			for (int i = 0; i < _N; i++) {
				_samples[i] *= (float)(0.54f - 0.46f * cos(2 * M_PI * i / N));
			}
		}

		void window_blackman(std::complex<float>* _samples, const int& _N) {
			int N = _N - 1;
			for (int n = 0; n < _N; n++) {
				_samples[n] *= (float)(0.42f - 0.5f * cos(2.f * M_PI * n / N) + 0.08f * cos(4.f * M_PI * n / N));
			}
		}

		/* *************************************************************************************************
			CREATE A FILTER KERNEL (IMPULSE RESPONSE) USED TO CONVOLVE THE SIGNAL WITH
			(FILTER SPECIFIC FREQUENCIES, E.G. ANYTHING FROM 3000HZ UPWARDS)
		************************************************************************************************* */
		// produces a window-sinc filter kernel for a given cutoff frequency and transition bandwidth
			// _cutoff < _sampling_rate / 2 && _cutoff > 0
			// _transition < _sampling_rate / 2 && _transition > 0
			// -> https://www.desmos.com/calculator/sc6otxqdul?lang=de
			// resources for the interested ones: 
			// https://ccrma.stanford.edu/~jos/sasp/Ideal_Lowpass_Filter.html
			// https://ccrma.stanford.edu/~jos/filters/Frequency_Response_I.html
			// https://ccrma.stanford.edu/~jos/sasp/Example_1_Low_Pass_Filtering.html
			// http://doctord.webhop.net/Courses/textbooks/Smith_DSP/dsp_book_Ch16.pdf
			// https://fiiir.com/
		void fn_window_sinc(std::vector<std::complex<float>>& _impulse_response, const int& _sampling_rate, const int& _f_cutoff, const TRANSITION_BANDWITH& _f_transtion, const bool& _high_pass) {
			float fc = (_high_pass ? ((_sampling_rate / 2) - (float)_f_cutoff) / _sampling_rate : (float)_f_cutoff / _sampling_rate);
			float BW = (float)_f_transtion / _sampling_rate;
			int N = (int)std::ceil(4 / BW);
			if (!(N % 2)) { N++; }
			_impulse_response.assign(N, std::complex<float>());

			for (int n = 0; n < N; n++) {
				if (n == ((N - 1) / 2)) {
					_impulse_response[n].real(2 * fc);		// lim x->0 2 * fc * sinc(2*fc*x) = 2 * fc (* 1)
				} else {
					_impulse_response[n].real((float)(sin(2 * M_PI * fc * (n - ((N - 1) / 2))) / (M_PI * (n - ((N - 1) / 2)))));
				}
			}

			window_tukey(_impulse_response.data(), (int)_impulse_response.size());

			float sum = .0f;
			for (const auto& n : _impulse_response) {
				sum += n.real();
			}
			for (auto& n : _impulse_response) {
				n /= sum;
			}

			if (_high_pass) {
				for (int i = 0; auto & n : _impulse_response) {
					n *= (float)(i % 2 ? -1 : 1);
				}
			}
		}
	}
}