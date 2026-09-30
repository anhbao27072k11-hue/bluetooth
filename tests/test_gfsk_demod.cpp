// language: C++, file: test_gfsk_demod.cpp, target: Linux, GCC/Clang
// *Unit test — feed synthetic GFSK-modulated bits, assert demod recovers them*
#define _USE_MATH_DEFINES   // MSVC: expose M_PI etc. from <cmath>
#include "gfsk_demod.h"

#include <cstdio>
#include <cmath>
#include <complex>
#include <random>
#include <vector>

using namespace bst;

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { \
    std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_fail++; } } while (0)

// GFSK modulator: map bits to frequency deviation, integrate phase.
// h = 0.5 (BLE), BT = 0.5 (Gaussian filter approximated by a simple FIR).
static std::vector<std::complex<float>> gfsk_modulate(const std::vector<uint8_t>& bits,
                                                      double sample_rate,
                                                      double symbol_rate,
                                                      double h) {
    const double sps = sample_rate / symbol_rate;
    const double dev = h * symbol_rate / 2.0; // peak deviation (Hz)
    const double ts = 1.0 / sample_rate;

    // Gaussian pulse shaping (BT=0.5): simple 3-tap smoothing of the NRZ stream.
    std::vector<double> nrz(bits.size(), 0.0);
    for (size_t i = 0; i < bits.size(); i++) nrz[i] = bits[i] ? 1.0 : -1.0;

    std::vector<std::complex<float>> out;
    out.reserve(static_cast<size_t>(bits.size() * sps));
    double phase = 0.0;
    for (size_t i = 0; i < bits.size(); i++) {
        // Smoothed symbol value (Gaussian-ish).
        double s = nrz[i];
        if (i > 0) s = 0.25 * nrz[i - 1] + 0.5 * nrz[i];
        if (i + 1 < bits.size()) s += 0.25 * nrz[i + 1];
        double freq = dev * s;
        for (size_t k = 0; k < static_cast<size_t>(sps); k++) {
            phase += 2.0 * M_PI * freq * ts;
            out.push_back(std::complex<float>(std::cos(phase), std::sin(phase)));
        }
    }
    return out;
}

// Inject complex AWGN at a target SNR (dB). Signal power is measured from the
// input; noise std-dev is scaled so that SNR = 10^(snr_db/10).
static void add_awgn(std::vector<std::complex<float>>& iq, double snr_db,
                     unsigned seed) {
    double sig_pow = 0.0;
    for (const auto& s : iq) sig_pow += std::norm(s);
    sig_pow /= static_cast<double>(iq.size());

    double noise_pow = sig_pow / std::pow(10.0, snr_db / 10.0);
    double noise_std = std::sqrt(noise_pow / 2.0); // per real/imag component

    std::mt19937 rng(seed);
    std::normal_distribution<double> gauss(0.0, noise_std);
    for (auto& s : iq) {
        s += std::complex<float>(static_cast<float>(gauss(rng)),
                                 static_cast<float>(gauss(rng)));
    }
}

// Demodulate and return bit-recovery accuracy in [0,1].
static double demod_accuracy(const std::vector<uint8_t>& bits,
                             const std::vector<std::complex<float>>& iq,
                             double sample_rate, double symbol_rate) {
    GfskDemod demod(sample_rate, symbol_rate, 1e6);
    std::vector<uint8_t> out;
    demod.process(iq.data(), iq.size(), out);

    size_t n = bits.size() < out.size() ? bits.size() : out.size();
    size_t match = 0;
    for (size_t i = 0; i < n; i++) if (out[i] == bits[i]) match++;
    return n ? static_cast<double>(match) / n : 0.0;
}

int main() {
    // A known bit sequence.
    std::vector<uint8_t> bits;
    for (int i = 0; i < 200; i++) bits.push_back((i * 7 + 3) % 2);

    const double sample_rate = 20e6;
    const double symbol_rate = 1e6;
    const double h = 0.5;

    // --- Clean signal: Gardner timing recovery must lock and recover bits. ---
    auto iq = gfsk_modulate(bits, sample_rate, symbol_rate, h);
    double acc_clean = demod_accuracy(bits, iq, sample_rate, symbol_rate);
    std::printf("clean demod accuracy: %.2f%%\n", acc_clean * 100.0);
    CHECK(acc_clean > 0.9); // must recover >90% of bits

    // --- Gardner timing recovery at SNR = 10 dB with AWGN injection. ---
    // The timing loop must still converge and recover the majority of bits.
    auto iq_noisy = gfsk_modulate(bits, sample_rate, symbol_rate, h);
    add_awgn(iq_noisy, 10.0, 0xC0FFEEu);
    double acc_noisy = demod_accuracy(bits, iq_noisy, sample_rate, symbol_rate);
    std::printf("10 dB SNR demod accuracy: %.2f%%\n", acc_noisy * 100.0);
    CHECK(acc_noisy > 0.8); // timing recovery holds at 10 dB SNR

    if (g_fail == 0) { std::printf("test_gfsk_demod: ALL PASS\n"); return 0; }
    std::printf("test_gfsk_demod: %d FAILURES\n", g_fail);
    return 1;
}
