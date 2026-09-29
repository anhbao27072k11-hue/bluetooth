// language: C++, file: dsp_compat.h, target: Windows/Linux, MSVC/MinGW/GCC
// *Self-contained DSP replacements — no external DSP library (liquid-dsp) needed*
//
// This module replaces the liquid-dsp calls that the original Linux build used
// in the GFSK demodulator. Every function here is a real, self-contained
// implementation with the underlying math documented. There is no dependency on
// liquid-dsp or any other DSP library.
//
// Replaced liquid-dsp functions:
//   firfilt_crcf_create_kaiser(num_taps, fc, beta)
//       -> dsp::FirFilter::design_kaiser(num_taps, fc, beta)
//          Kaiser-windowed lowpass FIR. Window coefficients use the modified
//          Bessel function of the first kind, order 0 (I0). The ideal lowpass
//          impulse response is sinc(n) * window(n), normalized so the DC gain
//          is 1.0.
//   firfilt_crcf_set_scale(fir, scale)
//       -> dsp::FirFilter::set_scale(scale)  (multiplies taps by a constant)
//   firfilt_crcf_execute(fir, x, &y)
//       -> dsp::FirFilter::execute(x)  (circular-buffer convolution)
//   firfilt_crcf_reset(fir)
//       -> dsp::FirFilter::reset()  (zero the delay line)
//   firfilt_crcf_destroy(fir)
//       -> dsp::FirFilter destructor (RAII)
//
// The FIR is a complex-in / complex-out filter (matching liquid's firfilt_crcf,
// which is complex-in, complex-out). The convolution is implemented with a
// circular delay line so it runs in O(num_taps) per sample regardless of how
// many samples are processed.
#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace bst {
namespace dsp {

// ---------------------------------------------------------------------------
// Modified Bessel function of the first kind, order 0: I0(x).
// Used by the Kaiser window. Series expansion, accurate for the argument
// ranges used in filter design (|x| <= ~40).
// ---------------------------------------------------------------------------
double bessel_i0(double x);

// ---------------------------------------------------------------------------
// Kaiser-windowed lowpass FIR filter (complex in / complex out).
//
// Design math:
//   * Ideal lowpass impulse response (infinite): h[n] = 2*fc*sinc(2*fc*n),
//     where fc is the normalized cutoff in [0, 0.5] (cycles/sample).
//   * Kaiser window: w[n] = I0(beta * sqrt(1 - ((n - M)/M)^2)) / I0(beta),
//     for n in [0, N-1], M = (N-1)/2.
//   * Final taps: h[n] * w[n], then normalized so the sum of taps = 1.0
//     (unity DC gain).
//
// This mirrors liquid-dsp's firfilt_crcf_create_kaiser(num_taps, fc, beta),
// where fc is the normalized cutoff (0..0.5) and beta is the Kaiser beta
// parameter (40.0 in the original GFSK demod).
// ---------------------------------------------------------------------------
class FirFilter {
public:
    // Design a Kaiser-windowed lowpass FIR.
    //   num_taps : number of taps (odd recommended)
    //   fc       : normalized cutoff frequency in [0, 0.5] (cycles/sample)
    //   beta     : Kaiser window beta parameter (>= 0)
    static FirFilter design_kaiser(size_t num_taps, double fc, double beta);

    // Default constructor: empty filter (identity-ish, outputs zeros until
    // taps are set). Provided so the demod can hold a default-constructed
    // member and assign later.
    FirFilter() = default;

    // Construct from an explicit tap vector (real taps applied to both I and Q).
    explicit FirFilter(std::vector<double> taps);

    // Multiply all taps by a constant (liquid's firfilt_crcf_set_scale).
    void set_scale(double scale);

    // Reset the internal delay line to zeros.
    void reset();

    // Process one complex sample, return the filtered output.
    // Circular-buffer convolution: y[n] = sum_k taps[k] * x[n-k].
    std::complex<float> execute(const std::complex<float>& x);

    // Number of taps.
    size_t num_taps() const { return taps_.size(); }

private:
    std::vector<double> taps_;      // real filter taps
    std::vector<std::complex<float>> delay_; // circular delay line
    size_t head_ = 0;               // index of the oldest sample in delay_
};

} // namespace dsp
} // namespace bst
