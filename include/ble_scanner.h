// language: C++, file: ble_scanner.h, target: Linux, GCC/Clang
// *BLE advertisement scanner — RX ring buffer + GFSK demod + BLE decode thread*
#pragma once

#include <cstdint>
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "sdr_device.h"
#include "ble_decoder.h"

namespace bst {

// BLE advertising channels (MHz)
constexpr double kAdvCh37 = 2402e6;
constexpr double kAdvCh38 = 2426e6;
constexpr double kAdvCh39 = 2480e6;

class BleScanner {
public:
    explicit BleScanner(SdrDevice& dev);
    ~BleScanner();

    // Scan all three adv channels in a round-robin loop until stop() is called.
    // Each channel is dwelled for dwell_ms before hopping.
    void start(uint32_t dwell_ms = 100);
    void stop();

    // Return decoded devices, de-duplicated by MAC (latest RSSI + timestamp).
    std::vector<BleAdv> get_devices();

private:
    // RX callback: push samples into the ring buffer (called on SDR thread).
    void on_samples(const SampleBuffer& samples);
    // Decode thread: pull from ring, run demod -> decoder.
    void decode_loop();

    SdrDevice& dev_;
    std::atomic<bool> running_{false};

    // Ring buffer (mutex-guarded deque).
    std::mutex ring_mtx_;
    std::deque<SampleBuffer> ring_;
    static constexpr size_t kMaxRing = 64;

    std::thread decode_thread_;

    // Decoded device cache, keyed by MAC.
    std::mutex dev_mtx_;
    std::vector<BleAdv> devices_;
};

} // namespace bst
