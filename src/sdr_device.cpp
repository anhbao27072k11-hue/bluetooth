// language: C++, file: sdr_device.cpp, target: Linux, GCC/Clang
// *Backend wrappers — libhackrf / libuhd / libbladerf behind SdrDevice*
#include "sdr_device.h"
#include "utils.h"

#include <atomic>
#include <thread>
#include <cmath>
#include <cstring>

#if defined(USE_HACKRF)
#include <libhackrf/hackrf.h>
#endif
#if defined(USE_UHD)
#include <uhd/usrp/multi_usrp.hpp>
#endif
#if defined(USE_BLADERF)
#include <libbladerf.h>
#endif

namespace bst {

// ---------------------------------------------------------------------------
// HackRF backend
// ---------------------------------------------------------------------------
#if defined(USE_HACKRF)
class HackrfDevice : public SdrDevice {
public:
    HackrfDevice() = default;
    ~HackrfDevice() override { close(); }

    int open(const SdrConfig& cfg) override {
        if (hackrf_init() != HACKRF_SUCCESS) return -1;
        if (cfg.device_serial.empty()) {
            if (hackrf_open(&dev_) != HACKRF_SUCCESS) return -2;
        } else {
            if (hackrf_open_by_serial(cfg.device_serial.c_str(), &dev_) != HACKRF_SUCCESS)
                return -2;
        }
        cfg_ = cfg;
        hackrf_set_freq(dev_, static_cast<uint64_t>(cfg.center_freq_hz));
        hackrf_set_sample_rate(dev_, static_cast<uint32_t>(cfg.sample_rate_hz));
        hackrf_set_lna_gain(dev_, static_cast<uint32_t>(cfg.gain_db));
        hackrf_set_vga_gain(dev_, 20);
        hackrf_set_amp_enable(dev_, 0);
        open_ = true;
        return 0;
    }

    void close() override {
        if (open_) {
            hackrf_close(dev_);
            hackrf_exit();
            open_ = false;
        }
    }

    int start_rx(SampleCallback cb) override {
        cb_ = std::move(cb);
        rx_running_ = true;
        rx_thread_ = std::thread([this]() {
            hackrf_start_rx(dev_, &HackrfDevice::rx_cb_static, this);
        });
        return 0;
    }

    // Called from the RX thread to deliver converted samples.
    void deliver(const SampleBuffer& out) {
        if (cb_) cb_(out);
    }

    void stop_rx() override {
        rx_running_ = false;
        hackrf_stop_rx(dev_);
        if (rx_thread_.joinable()) rx_thread_.join();
    }

    int transmit(const SampleBuffer& samples) override {
        // HackRF TX expects int8 interleaved I/Q. Convert from float.
        std::vector<int8_t> buf(samples.size() * 2);
        for (size_t i = 0; i < samples.size(); i++) {
            buf[2 * i]     = static_cast<int8_t>(samples[i].real() * 127.0f);
            buf[2 * i + 1] = static_cast<int8_t>(samples[i].imag() * 127.0f);
        }
        return hackrf_start_tx(dev_, &HackrfDevice::tx_cb_static, this) == HACKRF_SUCCESS ? 0 : -1;
    }

    int set_center_freq(double hz) override {
        return hackrf_set_freq(dev_, static_cast<uint64_t>(hz)) == HACKRF_SUCCESS ? 0 : -1;
    }

    bool is_open() const override { return open_; }
    std::string backend_name() const override { return "hackrf"; }

private:
    static int rx_cb_static(hackrf_transfer* t) {
        // t->buffer is int8 I/Q pairs; convert to complex<float>.
        auto* self = static_cast<HackrfDevice*>(t->rx_ctx);
        SampleBuffer out;
        out.reserve(t->valid_length / 2);
        for (int i = 0; i < t->valid_length; i += 2) {
            out.emplace_back(t->buffer[i] / 127.0f, t->buffer[i + 1] / 127.0f);
        }
        self->deliver(out);
        return 0;
    }

    static int tx_cb_static(hackrf_transfer* t) {
        // Fill TX buffer with silence (real TX handled by jammer loop).
        std::memset(t->buffer, 0, static_cast<size_t>(t->buffer_length));
        return 0;
    }

    hackrf_device* dev_ = nullptr;
    SdrConfig cfg_;
    bool open_ = false;
    std::atomic<bool> rx_running_{false};
    std::thread rx_thread_;
    SampleCallback cb_;
};
#endif

// ---------------------------------------------------------------------------
// USRP backend (libuhd)
// ---------------------------------------------------------------------------
#if defined(USE_UHD)
class UsrpDevice : public SdrDevice {
public:
    UsrpDevice() = default;
    ~UsrpDevice() override { close(); }

    int open(const SdrConfig& cfg) override {
        try {
            std::string args = cfg.device_serial.empty() ? "" : "serial=" + cfg.device_serial;
            usrp_ = uhd::usrp::multi_usrp::make(args);
            usrp_->set_rx_rate(cfg.sample_rate_hz);
            usrp_->set_tx_rate(cfg.sample_rate_hz);
            usrp_->set_rx_freq(cfg.center_freq_hz);
            usrp_->set_tx_freq(cfg.center_freq_hz);
            usrp_->set_rx_gain(cfg.gain_db);
            usrp_->set_tx_gain(cfg.gain_db);
            cfg_ = cfg;
            open_ = true;
            return 0;
        } catch (const std::exception& e) {
            LOG_E("usrp", std::string("open failed: ") + e.what());
            return -1;
        }
    }

    void close() override { open_ = false; usrp_.reset(); }

    int start_rx(SampleCallback cb) override {
        cb_ = std::move(cb);
        rx_running_ = true;
        rx_thread_ = std::thread([this]() {
            auto stream = usrp_->get_rx_stream(uhd::stream_args_t("fc32", "sc16"));
            uhd::stream_cmd_t cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
            cmd.stream_now = true;
            stream->issue_stream_cmd(cmd);
            SampleBuffer buf(cfg_.buffer_size);
            std::vector<std::complex<float>>* buffs = { &buf };
            while (rx_running_) {
                size_t got = stream->recv(buffs, buf.size(), uhd::rx_metadata_t{}, 1.0);
                buf.resize(got);
                if (cb_) cb_(buf);
                buf.resize(cfg_.buffer_size);
            }
        });
        return 0;
    }

    void stop_rx() override {
        rx_running_ = false;
        if (rx_thread_.joinable()) rx_thread_.join();
    }

    int transmit(const SampleBuffer& samples) override {
        auto stream = usrp_->get_tx_stream(uhd::stream_args_t("fc32", "sc16"));
        std::vector<std::complex<float>>* buffs = { const_cast<SampleBuffer*>(&samples) };
        stream->send(buffs, samples.size(), uhd::tx_metadata_t{}, 1.0);
        return 0;
    }

    int set_center_freq(double hz) override {
        usrp_->set_rx_freq(hz);
        usrp_->set_tx_freq(hz);
        return 0;
    }

    bool is_open() const override { return open_; }
    std::string backend_name() const override { return "usrp"; }

private:
    uhd::usrp::multi_usrp::sptr usrp_;
    SdrConfig cfg_;
    bool open_ = false;
    std::atomic<bool> rx_running_{false};
    std::thread rx_thread_;
    SampleCallback cb_;
};
#endif

// ---------------------------------------------------------------------------
// BladeRF backend (libbladerf)
// ---------------------------------------------------------------------------
#if defined(USE_BLADERF)
class BladeRfDevice : public SdrDevice {
public:
    BladeRfDevice() = default;
    ~BladeRfDevice() override { close(); }

    int open(const SdrConfig& cfg) override {
        if (cfg.device_serial.empty()) {
            if (bladerf_open(&dev_, nullptr) != 0) return -1;
        } else {
            if (bladerf_open(&dev_, cfg.device_serial.c_str()) != 0) return -1;
        }
        bladerf_set_frequency(dev_, BLADERF_MODULE_RX, static_cast<uint64_t>(cfg.center_freq_hz));
        bladerf_set_frequency(dev_, BLADERF_MODULE_TX, static_cast<uint64_t>(cfg.center_freq_hz));
        bladerf_set_sample_rate(dev_, BLADERF_MODULE_RX, static_cast<uint32_t>(cfg.sample_rate_hz), nullptr);
        bladerf_set_sample_rate(dev_, BLADERF_MODULE_TX, static_cast<uint32_t>(cfg.sample_rate_hz), nullptr);
        bladerf_set_gain(dev_, BLADERF_MODULE_RX, static_cast<int>(cfg.gain_db));
        bladerf_set_gain(dev_, BLADERF_MODULE_TX, static_cast<int>(cfg.gain_db));
        cfg_ = cfg;
        open_ = true;
        return 0;
    }

    void close() override {
        if (open_) { bladerf_close(dev_); open_ = false; }
    }

    int start_rx(SampleCallback cb) override {
        cb_ = std::move(cb);
        rx_running_ = true;
        rx_thread_ = std::thread([this]() {
            bladerf_sync_config(dev_, BLADERF_MODULE_RX, BLADERF_FORMAT_SC16_Q11,
                                cfg_.buffer_size, 16, 0, 10000);
            bladerf_enable_module(dev_, BLADERF_MODULE_RX, true);
            std::vector<int16_t> raw(cfg_.buffer_size * 2);
            SampleBuffer buf(cfg_.buffer_size);
            while (rx_running_) {
                int n = bladerf_sync_rx(dev_, raw.data(), cfg_.buffer_size, nullptr, 10000);
                if (n < 0) break;
                for (size_t i = 0; i < cfg_.buffer_size; i++) {
                    buf[i] = std::complex<float>(raw[2 * i] / 2048.0f, raw[2 * i + 1] / 2048.0f);
                }
                if (cb_) cb_(buf);
            }
            bladerf_enable_module(dev_, BLADERF_MODULE_RX, false);
        });
        return 0;
    }

    void stop_rx() override {
        rx_running_ = false;
        if (rx_thread_.joinable()) rx_thread_.join();
    }

    int transmit(const SampleBuffer& samples) override {
        std::vector<int16_t> raw(samples.size() * 2);
        for (size_t i = 0; i < samples.size(); i++) {
            raw[2 * i]     = static_cast<int16_t>(samples[i].real() * 2048.0f);
            raw[2 * i + 1] = static_cast<int16_t>(samples[i].imag() * 2048.0f);
        }
        bladerf_sync_config(dev_, BLADERF_MODULE_TX, BLADERF_FORMAT_SC16_Q11,
                            samples.size(), 16, 0, 10000);
        bladerf_enable_module(dev_, BLADERF_MODULE_TX, true);
        bladerf_sync_tx(dev_, raw.data(), samples.size(), nullptr, 10000);
        bladerf_enable_module(dev_, BLADERF_MODULE_TX, false);
        return 0;
    }

    int set_center_freq(double hz) override {
        bladerf_set_frequency(dev_, BLADERF_MODULE_RX, static_cast<uint64_t>(hz));
        bladerf_set_frequency(dev_, BLADERF_MODULE_TX, static_cast<uint64_t>(hz));
        return 0;
    }

    bool is_open() const override { return open_; }
    std::string backend_name() const override { return "bladerf"; }

private:
    struct bladerf* dev_ = nullptr;
    SdrConfig cfg_;
    bool open_ = false;
    std::atomic<bool> rx_running_{false};
    std::thread rx_thread_;
    SampleCallback cb_;
};
#endif

// ---------------------------------------------------------------------------
// Factory
// ---------------------------------------------------------------------------
std::unique_ptr<SdrDevice> SdrDevice::create(SdrBackend backend) {
    switch (backend) {
#if defined(USE_HACKRF)
        case SdrBackend::HackRF: return std::make_unique<HackrfDevice>();
#endif
#if defined(USE_UHD)
        case SdrBackend::Usrp:   return std::make_unique<UsrpDevice>();
#endif
#if defined(USE_BLADERF)
        case SdrBackend::BladeRf: return std::make_unique<BladeRfDevice>();
#endif
        default:
            LOG_E("sdr", "requested backend not compiled in");
            return nullptr;
    }
}

} // namespace bst
