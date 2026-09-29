// language: C++, file: test_jammer.cpp, target: Linux, GCC/Clang
// *Unit tests — jammer waveform synthesis, channel lists*
#include "jammer.h"
#include "frequency_hopper.h"

#include <cassert>
#include <cstdio>
#include <cmath>

static int g_fail = 0;
#define CHECK(cond) do { if (!(cond)) { \
    std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); g_fail++; } } while (0)

// A minimal fake SDR to exercise the jammer without hardware.
class FakeSdr : public bst::SdrDevice {
public:
    int open(const bst::SdrConfig&) override { open_ = true; return 0; }
    void close() override { open_ = false; }
    int start_rx(bst::SampleCallback) override { return 0; }
    void stop_rx() override {}
    int transmit(const bst::SampleBuffer& s) override { last_tx_ = s; return 0; }
    int set_center_freq(double f) override { last_freq_ = f; return 0; }
    bool is_open() const override { return open_; }
    std::string backend_name() const override { return "fake"; }

    bst::SampleBuffer last_tx_;
    double last_freq_ = 0;
    bool open_ = false;
};

static void test_channel_lists() {
    auto classic = bst::FrequencyHopper::classic_channels();
    CHECK(classic.size() == 79);
    CHECK(classic[0] == 2402e6);
    CHECK(classic[78] == 2480e6);

    auto ble = bst::FrequencyHopper::ble_channels();
    CHECK(ble.size() == 40);
    CHECK(ble[39] == 2480e6);

    auto adv = bst::FrequencyHopper::ble_adv_channels();
    CHECK(adv.size() == 3);
    CHECK(adv[0] == 2402e6 && adv[1] == 2426e6 && adv[2] == 2480e6);
}

static void test_tone_synthesis() {
    // Tone at 0 offset should be a constant complex value (amplitude-scaled).
    bst::JamConfig cfg;
    cfg.mode = bst::JamMode::Tone;
    cfg.tone_offset_hz = 0.0;
    cfg.amplitude = 0.5;
    auto buf = bst::Jammer::synthesize(cfg, 2440e6, 20e6, 100);
    CHECK(buf.size() == 100);
    // All samples should be (0.5, 0.0) for zero-offset tone.
    for (const auto& s : buf) {
        CHECK(std::fabs(s.real() - 0.5) < 1e-3);
        CHECK(std::fabs(s.imag()) < 1e-3);
    }
}

static void test_noise_synthesis() {
    bst::JamConfig cfg;
    cfg.mode = bst::JamMode::Noise;
    cfg.amplitude = 0.8;
    auto buf = bst::Jammer::synthesize(cfg, 2440e6, 20e6, 1000);
    CHECK(buf.size() == 1000);
    // Noise should have non-zero variance.
    double mean = 0, var = 0;
    for (const auto& s : buf) { mean += s.real(); }
    mean /= buf.size();
    for (const auto& s : buf) { var += (s.real() - mean) * (s.real() - mean); }
    var /= buf.size();
    CHECK(var > 1e-4);
}

static void test_jam_fixed() {
    FakeSdr sdr;
    bst::Jammer jammer(sdr);
    bst::JamConfig cfg;
    cfg.mode = bst::JamMode::Tone;
    cfg.amplitude = 0.5;
    // Run a short fixed jam then stop.
    jammer.jam_fixed(2440e6, cfg);
    // jam_fixed loops while running_; stop it.
    jammer.stop();
    CHECK(sdr.last_freq_ == 2440e6);
}

int main() {
    test_channel_lists();
    test_tone_synthesis();
    test_noise_synthesis();
    test_jam_fixed();

    if (g_fail == 0) { std::printf("test_jammer: ALL PASS\n"); return 0; }
    std::printf("test_jammer: %d FAILURES\n", g_fail);
    return 1;
}
