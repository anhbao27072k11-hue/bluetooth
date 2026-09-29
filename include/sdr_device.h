// language: C++, file: sdr_device.h, target: Linux, GCC/Clang
// *SDR hardware abstraction — HackRF / USRP / BladeRF backends behind one interface*
#pragma once

#include <cstdint>
#include <complex>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace bst {

// Backend selection
enum class SdrBackend { HackRF, Usrp, BladeRf };

// Complex sample buffer (interleaved I/Q as std::complex<float>)
using SampleBuffer = std::vector<std::complex<float>>;

// Callback fired when a block of samples is ready (RX path)
using SampleCallback = std::function<void(const SampleBuffer& samples)>;

struct SdrConfig {
    SdrBackend backend = SdrBackend::HackRF;
    double      center_freq_hz = 2440e6;   // default mid-band
    double      sample_rate_hz = 20e6;     // HackRF max 20 MSps
    double      gain_db = 40.0;
    uint32_t    buffer_size = 65536;       // samples per RX callback
    std::string device_serial;             // empty = first device
};

class SdrDevice {
public:
    virtual ~SdrDevice() = default;

    // Open the device, apply config. Returns 0 on success, negative errno-style on failure.
    virtual int open(const SdrConfig& cfg) = 0;
    virtual void close() = 0;

    // Start continuous RX streaming; samples delivered to cb on a worker thread.
    virtual int start_rx(SampleCallback cb) = 0;
    virtual void stop_rx() = 0;

    // Transmit a single burst of complex samples (jammer path).
    virtual int transmit(const SampleBuffer& samples) = 0;

    // Tune the center frequency live (hopping).
    virtual int set_center_freq(double hz) = 0;

    virtual bool is_open() const = 0;
    virtual std::string backend_name() const = 0;

    // Factory: build a device for the requested backend.
    static std::unique_ptr<SdrDevice> create(SdrBackend backend);
};

} // namespace bst
