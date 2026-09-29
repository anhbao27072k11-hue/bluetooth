// language: C++, file: frequency_hopper.cpp, target: Linux, GCC/Clang
#include "frequency_hopper.h"

#include <cmath>

namespace bst {

// xorshift32 — cheap PRNG for hop sequences.
static inline uint32_t xorshift32(uint32_t& s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

int FrequencyHopper::classic_hop(uint32_t& state) {
    // 79 channels: map a 7-bit value into 0..78, reject out-of-range.
    uint32_t v;
    do { v = xorshift32(state) & 0x7F; } while (v >= 79);
    return static_cast<int>(v);
}

int FrequencyHopper::ble_data_hop(uint32_t& state) {
    // 37 data channels (0..36).
    uint32_t v;
    do { v = xorshift32(state) & 0x3F; } while (v >= 37);
    return static_cast<int>(v);
}

double FrequencyHopper::classic_freq(int channel) {
    return kClassicStartHz + channel * kClassicSpacingHz;
}

double FrequencyHopper::ble_freq(int channel) {
    return kBleStartHz + channel * kBleSpacingHz;
}

std::vector<double> FrequencyHopper::classic_channels() {
    std::vector<double> v;
    v.reserve(kClassicChannels);
    for (int i = 0; i < kClassicChannels; i++) v.push_back(classic_freq(i));
    return v;
}

std::vector<double> FrequencyHopper::ble_channels() {
    std::vector<double> v;
    v.reserve(kBleChannels);
    for (int i = 0; i < kBleChannels; i++) v.push_back(ble_freq(i));
    return v;
}

std::vector<double> FrequencyHopper::ble_adv_channels() {
    return { ble_freq(kBleAdvCh37), ble_freq(kBleAdvCh38), ble_freq(kBleAdvCh39) };
}

} // namespace bst
