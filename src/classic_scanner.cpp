// language: C++, file: classic_scanner.cpp, target: Linux, GCC/Clang
#include "classic_scanner.h"
#include "frequency_hopper.h"
#include "gfsk_demod.h"
#include "classic_decoder.h"
#include "utils.h"

#include <algorithm>
#include <cmath>

namespace bst {

ClassicScanner::ClassicScanner(SdrDevice& dev) : dev_(dev) {}
ClassicScanner::~ClassicScanner() { stop(); }

void ClassicScanner::start(uint32_t dwell_ms) {
    if (running_) return;
    running_ = true;

    dev_.start_rx([this](const SampleBuffer& s) { on_samples(s); });
    decode_thread_ = std::thread([this]() { decode_loop(); });

    // Inquiry hop: 79 channels, 1 MHz spacing, 2402..2480 MHz.
    std::thread([this, dwell_ms]() {
        const auto chs = FrequencyHopper::classic_channels();
        size_t idx = 0;
        while (running_) {
            double freq = chs[idx % chs.size()];
            dev_.set_center_freq(freq);
            sleep_ms(dwell_ms);
            idx++;
        }
    }).detach();
}

void ClassicScanner::stop() {
    if (!running_) return;
    running_ = false;
    dev_.stop_rx();
    if (decode_thread_.joinable()) decode_thread_.join();
}

void ClassicScanner::on_samples(const SampleBuffer& samples) {
    std::lock_guard<std::mutex> lk(ring_mtx_);
    if (ring_.size() >= kMaxRing) ring_.pop_front();
    ring_.push_back(samples);
}

void ClassicScanner::decode_loop() {
    // Classic BR: 1 Mbps symbol rate, ~700 kHz cutoff.
    GfskDemod demod(20e6, 1e6, 700e3);
    ClassicDecoder decoder;

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

        std::vector<uint8_t> bits;
        demod.process(block.data(), block.size(), bits);

        double mag = 0;
        for (const auto& s : block) mag += std::abs(s);
        mag /= static_cast<double>(block.size());
        int8_t rssi = static_cast<int8_t>(20.0 * std::log10(mag + 1e-9));

        // Pack bits into bytes and attempt an FHS parse.
        std::vector<uint8_t> bytes((bits.size() + 7) / 8, 0);
        for (size_t i = 0; i < bits.size(); i++) {
            if (bits[i]) bytes[i / 8] |= static_cast<uint8_t>(1 << (i % 8));
        }

        for (size_t off = 0; off + 12 <= bytes.size(); off++) {
            ClassicDev dev;
            if (decoder.parse_fhs(bytes.data() + off, 12, rssi, dev)) {
                std::lock_guard<std::mutex> dlk(dev_mtx_);
                auto it = std::find_if(devices_.begin(), devices_.end(),
                                       [&](const ClassicDev& d) { return d.bd_addr == dev.bd_addr; });
                if (it != devices_.end()) *it = dev;
                else devices_.push_back(dev);
            }
        }
    }
}

std::vector<ClassicDev> ClassicScanner::get_devices() {
    std::lock_guard<std::mutex> lk(dev_mtx_);
    return devices_;
}

} // namespace bst
