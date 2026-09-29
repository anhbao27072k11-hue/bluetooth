// language: C++, file: jammer.h, target: Linux, GCC/Clang
// *Focused interference — noise injection or tone on selected channels*
#pragma once

#include <cstdint>
#include <atomic>
#include <string>
#include <vector>

#include "sdr_device.h"

namespace bst {

enum class JamMode {
    Noise,   // wideband Gaussian noise
    Tone,    // single CW tone at channel center
    Chirp,   // swept tone across the channel
};

struct JamConfig {
    JamMode mode = JamMode::Noise;
    double  bandwidth_hz = 2e6;   // noise bandwidth
    double  tone_offset_hz = 0.0; // tone offset from channel center
    double  amplitude = 0.8;      // 0..1 full-scale
    uint32_t dwell_ms = 2;        // time per channel before hopping
};

class Jammer {
public:
    explicit Jammer(SdrDevice& dev);
    ~Jammer();

    // Jam a single fixed frequency (e.g. one BLE data channel).
    void jam_fixed(double freq_hz, const JamConfig& cfg);

    // Jam a set of frequencies in a round-robin loop until stop().
    void jam_channels(const std::vector<double>& freqs, const JamConfig& cfg);

    void stop();

private:
    // Generate one burst of samples for the given mode/bandwidth.
    static SampleBuffer synthesize(const JamConfig& cfg, double center_hz,
                                   double sample_rate, size_t n);

    SdrDevice& dev_;
    std::atomic<bool> running_{false};
};

} // namespace bst
