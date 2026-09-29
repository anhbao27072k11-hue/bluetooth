// language: C++, file: main.cpp, target: Linux, GCC/Clang
// *Entry point + CLI parser — scan / jam / analyze modes*
#include "sdr_device.h"
#include "ble_scanner.h"
#include "classic_scanner.h"
#include "jammer.h"
#include "frequency_hopper.h"
#include "packet_analyzer.h"
#include "gfsk_demod.h"
#include "ble_decoder.h"
#include "platform.h"
#include "utils.h"

#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace bst;

static void print_usage() {
    std::cout <<
        "bluetooth_signal_tool — SDR Bluetooth recon + focused interference\n\n"
        "Usage:\n"
        "  bluetooth_signal_tool scan ble     [--backend hackrf|usrp|bladerf] [--gain DB] [--dwell MS]\n"
        "  bluetooth_signal_tool scan classic [--backend ...] [--gain DB] [--dwell MS]\n"
        "  bluetooth_signal_tool jam ble      [--backend ...] [--target MAC] [--mode noise|tone|chirp]\n"
        "  bluetooth_signal_tool jam classic  [--backend ...] [--mode ...] [--dwell-ms MS]\n"
        "  bluetooth_signal_tool analyze --file PATH\n\n"
        "Options:\n"
        "  --backend   SDR backend (default hackrf)\n"
        "  --gain      RX/TX gain in dB (default 40)\n"
        "  --dwell     dwell time per channel in ms (default 100)\n"
        "  --target    target MAC to focus jamming on\n"
        "  --mode      jam waveform: noise | tone | chirp (default noise)\n"
        "  --file      raw capture file for offline analysis\n";
}

static SdrBackend parse_backend(const std::string& s) {
    if (s == "usrp")   return SdrBackend::Usrp;
    if (s == "bladerf") return SdrBackend::BladeRf;
    return SdrBackend::HackRF;
}

static std::string get_opt(const std::vector<std::string>& args, const std::string& key,
                           const std::string& def = {}) {
    for (size_t i = 0; i + 1 < args.size(); i++) {
        if (args[i] == key) return args[i + 1];
    }
    return def;
}

static bool has_flag(const std::vector<std::string>& args, const std::string& key) {
    for (const auto& a : args) if (a == key) return true;
    return false;
}

static int run_scan_ble(const std::vector<std::string>& args) {
    SdrConfig cfg;
    cfg.backend = parse_backend(get_opt(args, "--backend", "hackrf"));
    cfg.gain_db = std::stod(get_opt(args, "--gain", "40"));
    uint32_t dwell = std::stoul(get_opt(args, "--dwell", "100"));

    auto dev = SdrDevice::create(cfg.backend);
    if (!dev || dev->open(cfg) != 0) { LOG_E("main", "failed to open SDR"); return 1; }

    BleScanner scanner(*dev);
    scanner.start(dwell);

    std::cout << "Scanning BLE adv channels (37/38/39)... Ctrl-C to stop\n";
    while (!platform::stop_requested()) {
        sleep_ms(500);
        for (const auto& adv : scanner.get_devices()) {
            std::cout << adv.timestamp_ms << " | " << adv.mac
                      << " | rssi=" << static_cast<int>(adv.rssi_dbm)
                      << " | name=\"" << adv.name << "\""
                      << " | type=" << static_cast<int>(adv.adv_type)
                      << " | ch" << static_cast<int>(adv.channel);
            if (!adv.services.empty()) {
                std::cout << " | svc=";
                for (size_t i = 0; i < adv.services.size(); i++) {
                    if (i) std::cout << ",";
                    std::cout << std::hex << adv.services[i] << std::dec;
                }
            }
            std::cout << "\n";
        }
    }
    scanner.stop();
    return 0;
}

static int run_scan_classic(const std::vector<std::string>& args) {
    SdrConfig cfg;
    cfg.backend = parse_backend(get_opt(args, "--backend", "hackrf"));
    cfg.gain_db = std::stod(get_opt(args, "--gain", "40"));
    uint32_t dwell = std::stoul(get_opt(args, "--dwell", "2"));

    auto dev = SdrDevice::create(cfg.backend);
    if (!dev || dev->open(cfg) != 0) { LOG_E("main", "failed to open SDR"); return 1; }

    ClassicScanner scanner(*dev);
    scanner.start(dwell);

    std::cout << "Running Classic inquiry (79 channels)... Ctrl-C to stop\n";
    while (!platform::stop_requested()) {
        sleep_ms(500);
        for (const auto& d : scanner.get_devices()) {
            std::cout << d.timestamp_ms << " | " << d.bd_addr
                      << " | CoD=0x" << std::hex << d.class_of_device << std::dec
                      << " (" << ClassicDev::major_class_name(ClassicDev::major_class(d.class_of_device))
                      << ")"
                      << " | rssi=" << static_cast<int>(d.rssi_dbm) << "\n";
        }
    }
    scanner.stop();
    return 0;
}

static int run_jam_ble(const std::vector<std::string>& args) {
    SdrConfig cfg;
    cfg.backend = parse_backend(get_opt(args, "--backend", "hackrf"));
    cfg.gain_db = std::stod(get_opt(args, "--gain", "40"));

    auto dev = SdrDevice::create(cfg.backend);
    if (!dev || dev->open(cfg) != 0) { LOG_E("main", "failed to open SDR"); return 1; }

    JamConfig jc;
    std::string mode = get_opt(args, "--mode", "noise");
    if (mode == "tone") jc.mode = JamMode::Tone;
    else if (mode == "chirp") jc.mode = JamMode::Chirp;
    else jc.mode = JamMode::Noise;

    Jammer jammer(*dev);
    std::string target = get_opt(args, "--target");
    if (!target.empty()) {
        // Focus on the BLE data channels (0..36) — jam all of them.
        auto chans = FrequencyHopper::ble_channels();
        chans.pop_back(); chans.pop_back(); chans.pop_back(); // drop adv 37/38/39
        std::cout << "Jamming BLE data channels for target " << target << "...\n";
        jammer.jam_channels(chans, jc);
    } else {
        auto adv = FrequencyHopper::ble_adv_channels();
        std::cout << "Jamming BLE advertising channels...\n";
        jammer.jam_channels(adv, jc);
    }

    while (!platform::stop_requested()) sleep_ms(100);
    jammer.stop();
    return 0;
}

static int run_jam_classic(const std::vector<std::string>& args) {
    SdrConfig cfg;
    cfg.backend = parse_backend(get_opt(args, "--backend", "hackrf"));
    cfg.gain_db = std::stod(get_opt(args, "--gain", "40"));

    auto dev = SdrDevice::create(cfg.backend);
    if (!dev || dev->open(cfg) != 0) { LOG_E("main", "failed to open SDR"); return 1; }

    JamConfig jc;
    std::string mode = get_opt(args, "--mode", "noise");
    if (mode == "tone") jc.mode = JamMode::Tone;
    else if (mode == "chirp") jc.mode = JamMode::Chirp;
    else jc.mode = JamMode::Noise;
    jc.dwell_ms = std::stoul(get_opt(args, "--dwell-ms", "2"));

    Jammer jammer(*dev);
    auto chans = FrequencyHopper::classic_channels();
    std::cout << "Jamming all 79 Classic channels...\n";
    jammer.jam_channels(chans, jc);

    while (!platform::stop_requested()) sleep_ms(100);
    jammer.stop();
    return 0;
}

static int run_analyze(const std::vector<std::string>& args) {
    std::string file = get_opt(args, "--file");
    if (file.empty()) { LOG_E("main", "--file required for analyze"); return 1; }
    std::string data;
    if (!read_file(file, data)) { LOG_E("main", "cannot read " + file); return 1; }

    // Offline analysis: treat the file as raw int8 I/Q samples (HackRF format).
    // Reconstruct complex samples and run the full GFSK demod + BLE decode path.
    size_t n = data.size() / 2;
    std::vector<std::complex<float>> iq(n);
    for (size_t i = 0; i < n; i++) {
        int8_t re = static_cast<int8_t>(data[2 * i]);
        int8_t im = static_cast<int8_t>(data[2 * i + 1]);
        iq[i] = std::complex<float>(re / 128.0f, im / 128.0f);
    }

    GfskDemod demod(20e6, 1e6, 1e6);
    BleDecoder decoder;
    std::vector<uint8_t> bits;
    demod.process(iq.data(), iq.size(), bits);

    int found = 0;
    for (uint8_t ch : {37, 38, 39}) {
        BleAdv adv;
        if (decoder.decode(bits, ch, 0, adv)) {
            std::cout << adv.timestamp_ms << " | " << adv.mac
                      << " | name=\"" << adv.name << "\""
                      << " | type=" << static_cast<int>(adv.adv_type) << "\n";
            found++;
        }
    }
    std::cout << "Analyzed " << data.size() << " bytes (" << iq.size()
              << " samples), decoded " << found << " adv packet(s)\n";
    return 0;
}

int main(int argc, char** argv) {
    platform::install_ctrl_c_handler();

    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) { print_usage(); return 0; }

    std::string mode = args[0];
    if (mode == "scan" && args.size() > 1 && args[1] == "ble")      return run_scan_ble(args);
    if (mode == "scan" && args.size() > 1 && args[1] == "classic")  return run_scan_classic(args);
    if (mode == "jam"  && args.size() > 1 && args[1] == "ble")      return run_jam_ble(args);
    if (mode == "jam"  && args.size() > 1 && args[1] == "classic")  return run_jam_classic(args);
    if (mode == "analyze")                                          return run_analyze(args);

    print_usage();
    return 0;
}
