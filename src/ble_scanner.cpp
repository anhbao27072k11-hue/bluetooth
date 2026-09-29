// language: C++, file: ble_scanner.cpp, target: Linux, GCC/Clang
#include "ble_scanner.h"
#include "frequency_hopper.h"
#include "gfsk_demod.h"
#include "ble_decoder.h"
#include "utils.h"

#include <algorithm>
#include <cmath>

namespace bst {

BleScanner::BleScanner(SdrDevice& dev) : dev_(dev) {}
BleScanner::~BleScanner() { stop(); }

void BleScanner::start(uint32_t dwell_ms) {
    if (running_) return;
    running_ = true;

    // Start RX streaming; samples land in on_samples via the SDR callback.
    dev_.start_rx([this](const SampleBuffer& s) { on_samples(s); });

    // Decode thread.
    decode_thread_ = std::thread([this]() { decode_loop(); });

    // Channel-hopping thread.
    std::thread([this, dwell_ms]() {
        const auto adv = FrequencyHopper::ble_adv_channels(); // 37/38/39
        size_t idx = 0;
        while (running_) {
            double freq = adv[idx % adv.size()];
            dev_.set_center_freq(freq);
            sleep_ms(dwell_ms);
            idx++;
        }
    }).detach();
}

void BleScanner::stop() {
    if (!running_) return;
    running_ = false;
    dev_.stop_rx();
    if (decode_thread_.joinable()) decode_thread_.join();
}

void BleScanner::on_samples(const SampleBuffer& samples) {
    std::lock_guard<std::mutex> lk(ring_mtx_);
    if (ring_.size() >= kMaxRing) ring_.pop_front();
    ring_.push_back(samples);
}

void BleScanner::decode_loop() {
    // GFSK demod at 1 Msps symbol rate, ~1 MHz cutoff for BLE.
    GfskDemod demod(20e6, 1e6, 1e6);
    BleDecoder decoder;

    while (running_) {
        SampleBuffer block;
        {
            std::lock_guard<std::mutex> lk(ring_mtx_);
            if (ring_.empty()) {
                sleep_ms(1);
                continue;
            }
            block = std::move(ring_.front());
            ring_.pop_front();
        }

        // Demodulate to a bitstream.
        std::vector<uint8_t> bits;
        demod.process(block.data(), block.size(), bits);

        // RSSI estimate: average magnitude of the input block, mapped to dBm.
        double mag = 0;
        for (const auto& s : block) mag += std::abs(s);
        mag /= static_cast<double>(block.size());
        int8_t rssi = static_cast<int8_t>(20.0 * std::log10(mag + 1e-9));

        // Attempt decode on all three adv channels (the hop thread sets the
        // center freq; we try each channel's whitening seed).
        for (uint8_t ch : {37, 38, 39}) {
            BleAdv adv;
            if (decoder.decode(bits, ch, rssi, adv)) {
                std::lock_guard<std::mutex> dlk(dev_mtx_);
                // De-dup by MAC: keep latest.
                auto it = std::find_if(devices_.begin(), devices_.end(),
                                       [&](const BleAdv& d) { return d.mac == adv.mac; });
                if (it != devices_.end()) *it = adv;
                else devices_.push_back(adv);
            }
        }
    }
}

std::vector<BleAdv> BleScanner::get_devices() {
    std::lock_guard<std::mutex> lk(dev_mtx_);
    return devices_;
}

} // namespace bst
