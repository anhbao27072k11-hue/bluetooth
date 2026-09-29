// language: C++, file: frequency_hopper.h, target: Linux, GCC/Clang
// *Frequency hopping — Bluetooth channel maps for Classic and BLE*
#pragma once

#include <cstdint>
#include <vector>

namespace bst {

// Bluetooth Classic: 79 channels, 1 MHz spacing, 2402–2480 MHz
constexpr int    kClassicChannels = 79;
constexpr double kClassicStartHz   = 2402e6;
constexpr double kClassicSpacingHz = 1e6;

// BLE: 40 channels, 2 MHz spacing, 2402–2480 MHz
constexpr int    kBleChannels   = 40;
constexpr double kBleStartHz    = 2402e6;
constexpr double kBleSpacingHz  = 2e6;

// BLE advertising channels (indices into the 40-channel map)
constexpr int kBleAdvCh37 = 37;
constexpr int kBleAdvCh38 = 38;
constexpr int kBleAdvCh39 = 39;

class FrequencyHopper {
public:
    // Classic AFH: pseudo-random hop over the 79 channels.
    // seed = master clock bits; returns the next channel index.
    static int classic_hop(uint32_t& state);

    // BLE data channel hop: 37 data channels (0..36) pseudo-random.
    static int ble_data_hop(uint32_t& state);

    // Convert a channel index to its center frequency in Hz.
    static double classic_freq(int channel);
    static double ble_freq(int channel);

    // Build the full channel list for a mode.
    static std::vector<double> classic_channels();
    static std::vector<double> ble_channels();
    static std::vector<double> ble_adv_channels();
};

} // namespace bst
