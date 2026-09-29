// language: C++, file: classic_scanner.h, target: Linux, GCC/Clang
// *Bluetooth Classic inquiry scanner — 79-channel hop, FHS decode*
#pragma once

#include <cstdint>
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "sdr_device.h"
#include "classic_decoder.h"

namespace bst {

class ClassicScanner {
public:
    explicit ClassicScanner(SdrDevice& dev);
    ~ClassicScanner();

    // Run inquiry: hop the 79 channels, listen for FHS packets, decode devices.
    void start(uint32_t dwell_ms = 2);
    void stop();

    // Return decoded devices, de-duplicated by BD_ADDR.
    std::vector<ClassicDev> get_devices();

private:
    void on_samples(const SampleBuffer& samples);
    void decode_loop();

    SdrDevice& dev_;
    std::atomic<bool> running_{false};

    std::mutex ring_mtx_;
    std::deque<SampleBuffer> ring_;
    static constexpr size_t kMaxRing = 64;

    std::thread decode_thread_;

    std::mutex dev_mtx_;
    std::vector<ClassicDev> devices_;
};

} // namespace bst
