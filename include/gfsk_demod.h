// language: C++, file: gfsk_demod.h, target: Linux, GCC/Clang
// *GFSK demodulator — DC block, IQ correction, FIR, discriminator, timing, slicer*
#pragma once

#include <cstdint>
#include <complex>
#include <vector>

#include "dsp_compat.h"

namespace bst {

// GFSK demodulator operating on complex baseband samples.
// Chain: DC block -> IQ imbalance correction -> FIR lowpass -> discriminator
//        -> Gardner timing recovery -> bit slicer -> packed bitstream.
class GfskDemod {
public:
    // sample_rate: input sample rate (Hz)
    // symbol_rate: 1e6 for BLE/Classic
    // cutoff_hz:   FIR cutoff (~1 MHz BLE, ~700 kHz Classic)
    GfskDemod(double sample_rate, double symbol_rate, double cutoff_hz);
    ~GfskDemod();

    // Feed a block of complex samples. Appends demodulated bits to out.
    // Returns number of bits produced.
    size_t process(const std::complex<float>* samples, size_t n, std::vector<uint8_t>& out);

    // Reset all internal state (DC estimate, timing loop, filters).
    void reset();

private:
    // DC block: running mean subtract.
    std::complex<double> dc_est_{0, 0};
    double dc_alpha_ = 0.001;

    // IQ imbalance correction state.
    double iq_gain_ = 1.0;
    double iq_phase_ = 0.0;

    // FIR lowpass (complex in/out) — self-contained Kaiser-windowed FIR.
    dsp::FirFilter fir_;

    // Timing recovery (Gardner) state.
    double timing_ = 0.0;      // fractional sample offset
    double timing_gain_ = 0.05;
    std::complex<float> prev_mid_{0, 0}; // previous mid-sample for Gardner
    std::complex<float> prev_prev_{0, 0};
    bool have_prev_ = false;

    // Discriminator state.
    std::complex<float> prev_sample_{0, 0};
    bool have_prev_sample_ = false;

    // Slicer state.
    double slice_threshold_ = 0.0;
    double slice_hysteresis_ = 0.05;

    double sample_rate_;
    double symbol_rate_;
    double cutoff_hz_;
    size_t sps_; // samples per symbol (integer)
};

} // namespace bst
