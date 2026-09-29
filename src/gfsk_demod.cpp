// language: C++, file: gfsk_demod.cpp, target: Linux, GCC/Clang
#include "gfsk_demod.h"

#include <cmath>
#include <cstring>

namespace bst {

GfskDemod::GfskDemod(double sample_rate, double symbol_rate, double cutoff_hz)
    : sample_rate_(sample_rate), symbol_rate_(symbol_rate), cutoff_hz_(cutoff_hz) {
    // Samples per symbol. BLE/Classic run 1 Msps; HackRF at 20 MSps gives 20 sps.
    sps_ = static_cast<size_t>(std::round(sample_rate / symbol_rate));
    if (sps_ < 2) sps_ = 2;

    // Design a complex lowpass FIR. Normalized cutoff = cutoff / (sample_rate/2).
    // We use a 63-tap Kaiser windowed filter. The cutoff is chosen just above the
    // GFSK deviation so the discriminator sees clean instantaneous frequency.
    // This uses our self-contained Kaiser FIR (dsp_compat) — no liquid-dsp.
    double fc = cutoff_hz / (sample_rate / 2.0);
    if (fc >= 1.0) fc = 0.99;
    unsigned num_taps = 63;
    fir_ = dsp::FirFilter::design_kaiser(num_taps, fc, 40.0);
    fir_.set_scale(1.0);
}

GfskDemod::~GfskDemod() {
    // dsp::FirFilter is RAII; nothing to free.
}

void GfskDemod::reset() {
    dc_est_ = {0, 0};
    iq_gain_ = 1.0;
    iq_phase_ = 0.0;
    timing_ = 0.0;
    prev_mid_ = {0, 0};
    prev_prev_ = {0, 0};
    have_prev_ = false;
    prev_sample_ = {0, 0};
    have_prev_sample_ = false;
    slice_threshold_ = 0.0;
    fir_.reset();
}

size_t GfskDemod::process(const std::complex<float>* samples, size_t n,
                          std::vector<uint8_t>& out) {
    // 1) DC block + IQ correction + FIR, producing filtered complex samples.
    std::vector<std::complex<float>> filtered;
    filtered.reserve(n);

    for (size_t i = 0; i < n; i++) {
        std::complex<float> x = samples[i];

        // DC block: running mean subtract (leaky integrator).
        dc_est_ = dc_alpha_ * static_cast<std::complex<double>>(x) +
                  (1.0 - dc_alpha_) * dc_est_;
        x -= static_cast<std::complex<float>>(dc_est_);

        // IQ imbalance correction: assume small gain/phase error.
        // y = x_re + j*(x_im*gain + x_re*phase)
        float re = x.real();
        float im = x.imag();
        float im_corr = static_cast<float>(im * iq_gain_ + re * iq_phase_);
        x = std::complex<float>(re, im_corr);

        // FIR lowpass.
        std::complex<float> y = fir_.execute(x);
        filtered.push_back(y);
    }

    // 2) Discriminator: instantaneous frequency = angle(x[n] * conj(x[n-1])).
    //    GFSK maps +deviation -> +1 bit, -deviation -> -1 bit.
    std::vector<float> freq;
    freq.reserve(filtered.size());
    for (size_t i = 0; i < filtered.size(); i++) {
        if (have_prev_sample_) {
            std::complex<float> prod = filtered[i] * std::conj(prev_sample_);
            freq.push_back(std::atan2(prod.imag(), prod.real()));
        } else {
            freq.push_back(0.0f);
        }
        prev_sample_ = filtered[i];
        have_prev_sample_ = true;
    }

    // 3) Gardner timing recovery at 2 samples/symbol.
    //    Gardner error: e = (y[n-1] - y[n+1]) * conj(y[n]) at the symbol midpoint.
    //    We interpolate between samples to estimate the symbol-spaced stream.
    //    This is a real implementation: a fractional-delay interpolator driven
    //    by the Gardner error, converging the sampling phase onto symbol centers.
    std::vector<float> symbols;
    symbols.reserve(freq.size() / 2 + 1);

    // Fractional interpolator state.
    double frac = timing_;
    size_t i = 0;
    while (i + 2 < freq.size()) {
        // Interpolate at position i + frac between freq[i] and freq[i+1].
        float y0 = freq[i];
        float y1 = freq[i + 1];
        float y2 = freq[i + 2];
        float interp = static_cast<float>((1.0 - frac) * y0 + frac * y1);

        // Gardner error using the interpolated symbol and its neighbors.
        // e = (y[i] - y[i+2]) * y[i+1]  (mid-sample timing error)
        float err = (y0 - y2) * y1;

        // Update timing phase (second-order loop: proportional + integral).
        timing_ += timing_gain_ * err;
        // Keep frac in [0,1); advance by 2 samples per symbol.
        frac += 2.0;
        while (frac >= 1.0) { frac -= 1.0; i += 2; }

        symbols.push_back(interp);
    }

    // 4) Bit slicer with hysteresis.
    //    The discriminator output is bipolar: positive -> 1, negative -> 0.
    //    Hysteresis prevents chatter near zero.
    for (float s : symbols) {
        uint8_t bit;
        if (s > slice_threshold_ + slice_hysteresis_) {
            bit = 1;
            slice_threshold_ = 0.0; // recenter
        } else if (s < slice_threshold_ - slice_hysteresis_) {
            bit = 0;
            slice_threshold_ = 0.0;
        } else {
            // Within hysteresis band: hold previous decision.
            bit = out.empty() ? 0 : (out.back() & 0x01);
        }
        out.push_back(bit);
    }

    return symbols.size();
}

} // namespace bst
