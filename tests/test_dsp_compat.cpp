// language: C++, file: test_dsp_compat.cpp, target: Windows/Linux, MSVC/MinGW/GCC
// *Unit test — verify the self-contained Kaiser FIR (dsp_compat) mathematically*
//
// This test validates the replacement for liquid-dsp's firfilt_crcf:
//   1. Kaiser window design produces a lowpass with unity DC gain.
//   2. The circular-buffer convolution matches a direct (naive) convolution.
//   3. The filter actually lowpass-filters: a DC signal passes, a high-frequency
//      signal (above cutoff) is attenuated.
//   4. bessel_i0 matches known reference values.
#include "dsp_compat.h"

#include <cstdio>
#include <cmath>
#include <complex>
#include <vector>

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { \
    std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_fail++; } } while (0)

// Reference: naive direct convolution (no circular buffer) for cross-checking.
static std::complex<float> direct_conv(const std::vector<double>& taps,
                                       const std::vector<std::complex<float>>& x,
                                       size_t n) {
    std::complex<double> acc(0.0, 0.0);
    for (size_t k = 0; k < taps.size(); k++) {
        if (n >= k) acc += taps[k] * static_cast<std::complex<double>>(x[n - k]);
    }
    return std::complex<float>(static_cast<float>(acc.real()),
                               static_cast<float>(acc.imag()));
}

int main() {
    // --- 1. bessel_i0 reference values (from standard tables). ---
    // I0(0) = 1, I0(1) ~= 1.266065877752008, I0(2) ~= 2.279585302336067.
    CHECK(std::abs(bst::dsp::bessel_i0(0.0) - 1.0) < 1e-9);
    CHECK(std::abs(bst::dsp::bessel_i0(1.0) - 1.266065877752008) < 1e-6);
    CHECK(std::abs(bst::dsp::bessel_i0(2.0) - 2.279585302336067) < 1e-6);

    // --- 2. Kaiser lowpass: unity DC gain. ---
    // A 63-tap Kaiser filter with fc=0.25, beta=40.0.
    auto fir = bst::dsp::FirFilter::design_kaiser(63, 0.25, 40.0);
    CHECK(fir.num_taps() == 63);

    // DC gain = sum of taps. Should be ~1.0 after normalization.
    // We can't read taps directly, but we can verify by feeding a constant
    // signal: after the filter settles, output should equal the input.
    std::complex<float> dc_in(1.0f, 0.0f);
    std::complex<float> dc_out(0.0f, 0.0f);
    for (int i = 0; i < 200; i++) dc_out = fir.execute(dc_in);
    CHECK(std::abs(dc_out.real() - 1.0f) < 1e-3);
    CHECK(std::abs(dc_out.imag()) < 1e-3);

    // --- 3. Circular-buffer convolution matches direct convolution. ---
    // Feed a known sequence and compare each output to the naive convolution.
    std::vector<std::complex<float>> input;
    for (int i = 0; i < 50; i++) {
        input.emplace_back(std::sin(0.3 * i), std::cos(0.2 * i));
    }
    // Rebuild taps for the direct reference (design again, same params).
    // We need the taps; expose via a small trick: run a unit impulse through
    // the filter and read the impulse response, which equals the taps.
    std::vector<double> taps(15, 0.0);
    {
        auto imp = bst::dsp::FirFilter::design_kaiser(15, 0.2, 5.0);
        imp.reset();
        std::complex<float> impulse(1.0f, 0.0f);
        for (size_t k = 0; k < 15; k++) {
            std::complex<float> y = imp.execute(impulse);
            taps[k] = y.real();
            impulse = std::complex<float>(0.0f, 0.0f);
        }
    }
    // Now compare fir2's output to direct_conv using the recovered taps.
    auto fir3 = bst::dsp::FirFilter::design_kaiser(15, 0.2, 5.0);
    for (size_t n = 0; n < input.size(); n++) {
        std::complex<float> got = fir3.execute(input[n]);
        std::complex<float> want = direct_conv(taps, input, n);
        CHECK(std::abs(got - want) < 1e-4);
    }

    // --- 4. Lowpass behavior: high-frequency signal is attenuated. ---
    // A tone at normalized frequency 0.4 (above fc=0.2) should be strongly
    // attenuated relative to a DC signal.
    auto lp = bst::dsp::FirFilter::design_kaiser(63, 0.2, 40.0);
    // Measure steady-state amplitude of a 0.4-cycle/sample tone.
    double peak = 0.0;
    for (int i = 0; i < 400; i++) {
        std::complex<float> s(std::cos(2.0 * M_PI * 0.4 * i),
                              std::sin(2.0 * M_PI * 0.4 * i));
        std::complex<float> y = lp.execute(s);
        double mag = std::abs(y);
        if (mag > peak) peak = mag;
    }
    // A 0.4 tone is well into the stopband; expect strong attenuation (< 0.1).
    CHECK(peak < 0.1);

    // --- 5. reset() zeroes the delay line. ---
    auto fr = bst::dsp::FirFilter::design_kaiser(7, 0.3, 5.0);
    for (int i = 0; i < 10; i++) fr.execute(std::complex<float>(1.0f, 1.0f));
    fr.reset();
    // After reset, feeding zeros yields zero output.
    std::complex<float> z = fr.execute(std::complex<float>(0.0f, 0.0f));
    CHECK(std::abs(z) < 1e-6);

    if (g_fail == 0) { std::printf("test_dsp_compat: ALL PASS\n"); return 0; }
    std::printf("test_dsp_compat: %d FAILURES\n", g_fail);
    return 1;
}
