// language: C++, file: dsp_compat.cpp, target: Windows/Linux, MSVC/MinGW/GCC
// *Self-contained DSP replacements — see dsp_compat.h for the math docs*
#define _USE_MATH_DEFINES   // MSVC: expose M_PI etc. from <cmath>
#include "dsp_compat.h"

#include <cmath>
#include <stdexcept>

namespace bst {
namespace dsp {

// ---------------------------------------------------------------------------
// Modified Bessel function of the first kind, order 0: I0(x).
//
// Series: I0(x) = sum_{k=0..inf} ( (x/2)^(2k) / (k!)^2 )
// The series converges rapidly for the argument range used in Kaiser window
// design (|x| <= ~40). We iterate until the term contribution is negligible
// relative to the running sum.
// ---------------------------------------------------------------------------
double bessel_i0(double x) {
    const double x2 = x * x;
    double sum = 1.0;
    double term = 1.0;
    for (int k = 1; k < 200; k++) {
        term *= x2 / (4.0 * static_cast<double>(k * k));
        sum += term;
        if (term < 1e-15 * sum) break;
    }
    return sum;
}

// ---------------------------------------------------------------------------
// Kaiser-windowed lowpass FIR design.
//
//   M = (num_taps - 1) / 2
//   ideal h[n] = 2*fc*sinc(2*fc*(n - M))          (centered, causal)
//   window w[n] = I0(beta*sqrt(1 - ((n-M)/M)^2)) / I0(beta)
//   taps[n] = h[n] * w[n]
//   normalize so sum(taps) = 1.0  (unity DC gain)
//
// This is the standard windowed-sinc lowpass design, equivalent to liquid-dsp's
// firfilt_crcf_create_kaiser(num_taps, fc, beta).
// ---------------------------------------------------------------------------
FirFilter FirFilter::design_kaiser(size_t num_taps, double fc, double beta) {
    if (num_taps < 1) num_taps = 1;
    if (fc < 0.0) fc = 0.0;
    if (fc > 0.5) fc = 0.5;

    const double M = static_cast<double>(num_taps - 1) / 2.0;
    const double i0_beta = bessel_i0(beta);

    std::vector<double> taps(num_taps);
    double sum = 0.0;
    for (size_t n = 0; n < num_taps; n++) {
        const double dn = static_cast<double>(n) - M; // centered index

        // Ideal lowpass impulse response: 2*fc*sinc(2*fc*dn).
        double h;
        if (std::abs(dn) < 1e-12) {
            h = 2.0 * fc;
        } else {
            const double arg = 2.0 * fc * dn;
            h = 2.0 * fc * std::sin(M_PI * arg) / (M_PI * arg);
        }

        // Kaiser window.
        double w;
        if (M < 1e-12) {
            w = 1.0;
        } else {
            const double r = dn / M;
            const double arg2 = 1.0 - r * r;
            w = bessel_i0(beta * std::sqrt(arg2 > 0.0 ? arg2 : 0.0)) / i0_beta;
        }

        taps[n] = h * w;
        sum += taps[n];
    }

    // Normalize to unity DC gain.
    if (std::abs(sum) > 1e-15) {
        for (auto& t : taps) t /= sum;
    }

    return FirFilter(std::move(taps));
}

FirFilter::FirFilter(std::vector<double> taps)
    : taps_(std::move(taps)) {
    if (!taps_.empty()) {
        delay_.assign(taps_.size(), std::complex<float>(0.0f, 0.0f));
        head_ = 0;
    }
}

void FirFilter::set_scale(double scale) {
    for (auto& t : taps_) t *= scale;
}

void FirFilter::reset() {
    if (!delay_.empty()) {
        std::fill(delay_.begin(), delay_.end(), std::complex<float>(0.0f, 0.0f));
        head_ = 0;
    }
}

std::complex<float> FirFilter::execute(const std::complex<float>& x) {
    const size_t N = taps_.size();
    if (N == 0) return x; // empty filter: pass-through

    // Write the new sample into the delay line at the head position.
    delay_[head_] = x;

    // Convolve: y = sum_k taps[k] * delay[(head - k) mod N].
    // Walking backward from head gives taps[0]*x[n] + taps[1]*x[n-1] + ...
    std::complex<double> acc(0.0, 0.0);
    size_t idx = head_;
    for (size_t k = 0; k < N; k++) {
        acc += static_cast<double>(taps_[k]) * static_cast<std::complex<double>>(delay_[idx]);
        idx = (idx == 0) ? (N - 1) : (idx - 1);
    }

    // Advance head to the next slot (oldest sample will be overwritten next).
    head_ = (head_ + 1) % N;

    return std::complex<float>(static_cast<float>(acc.real()),
                               static_cast<float>(acc.imag()));
}

} // namespace dsp
} // namespace bst
