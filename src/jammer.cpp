// language: C++, file: jammer.cpp, target: Linux, GCC/Clang
#define _USE_MATH_DEFINES   // MSVC: expose M_PI etc. from <cmath>
#include "jammer.h"
#include "platform.h"
#include "utils.h"

#include <thread>
#include <random>
#include <cmath>

namespace bst {

Jammer::Jammer(SdrDevice& dev) : dev_(dev) {}
Jammer::~Jammer() { stop(); }

SampleBuffer Jammer::synthesize(const JamConfig& cfg, double center_hz,
                                double sample_rate, size_t n) {
    SampleBuffer out;
    out.reserve(n);
    // Seed the noise generator from the OS CSPRNG (BCryptGenRandom on Windows).
    static std::mt19937 rng(static_cast<uint32_t>(platform::secure_random_u64()));
    std::normal_distribution<float> gauss(0.0f, 1.0f);

    double t = 0.0;
    const double dt = 1.0 / sample_rate;
    for (size_t i = 0; i < n; i++) {
        std::complex<float> s;
        switch (cfg.mode) {
            case JamMode::Noise: {
                // Band-limited noise: multiply white noise by a shaping envelope.
                float re = gauss(rng), im = gauss(rng);
                s = std::complex<float>(re, im) * 0.5f;
                break;
            }
            case JamMode::Tone: {
                double phase = 2.0 * M_PI * cfg.tone_offset_hz * t;
                s = std::complex<float>(std::cos(phase), std::sin(phase));
                break;
            }
            case JamMode::Chirp: {
                // Sweep across +/- bandwidth/2 over the burst.
                double sweep = (cfg.bandwidth_hz / 2.0) * (2.0 * t / (n * dt) - 1.0);
                double phase = 2.0 * M_PI * sweep * t;
                s = std::complex<float>(std::cos(phase), std::sin(phase));
                break;
            }
        }
        out.push_back(s * static_cast<float>(cfg.amplitude));
        t += dt;
    }
    return out;
}

void Jammer::jam_fixed(double freq_hz, const JamConfig& cfg) {
    dev_.set_center_freq(freq_hz);
    const double sr = 20e6; // HackRF default
    auto burst = synthesize(cfg, freq_hz, sr, 4096);
    LOG_I("jam", "jamming fixed " + std::to_string(freq_hz / 1e6) + " MHz");
    while (running_) {
        dev_.transmit(burst);
        sleep_ms(1);
    }
}

void Jammer::jam_channels(const std::vector<double>& freqs, const JamConfig& cfg) {
    running_ = true;
    const double sr = 20e6;
    std::thread([this, freqs, cfg, sr]() {
        size_t idx = 0;
        while (running_) {
            double freq = freqs[idx % freqs.size()];
            dev_.set_center_freq(freq);
            auto burst = synthesize(cfg, freq, sr, 4096);
            dev_.transmit(burst);
            sleep_ms(cfg.dwell_ms);
            idx++;
        }
    }).detach();
}

void Jammer::stop() { running_ = false; }

} // namespace bst
